/*
 * Custom Malloc Allocator
 *
 * Implements malloc, free, realloc, and calloc using segregated explicit free lists. 
 * Blocks are 16-byte aligned and store size/allocation metadata in both a header and footer.
 *
 * Free blocks use their payload space for next/previous free-list pointers.
 * The allocator splits large free blocks, coalesces adjacent free blocks, and
 * uses a best-fit search within size classes to reduce fragmentation.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"
 
//#define DEBUG

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
#define dbg_printf(...)
#define dbg_assert(...)
#endif // DEBUG

#ifdef DRIVER
// For driver tests
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mm_memset
#define memcpy mm_memcpy
#endif // DRIVER

#define WSIZE 8 // Word size, header/footer (bytes)
#define DSIZE 16 // Double word size, header + footer, for alignment
#define CHUNKSIZE 1024 // Default heap extension size
#define MIN_BLOCK_SIZE 32 // Min block size
#define NUM_CLASSES 14 // Number of segregated free list classes

static char *heap_listp = NULL; // Pointer to the prologue block payload
static void *seg_free_lists[NUM_CLASSES]; // Heads of segregated free list

bool mm_checkheap(int line_number);

static size_t align(size_t size);
static size_t max_size(size_t a, size_t b);
static size_t pack(size_t size, bool allocate);

static size_t get_word(const void* address);
static void put_word(void *address, size_t value);

static size_t get_size(const void *address);
static bool get_alloc(const void *address);

static void *hdrp(void *blockp);
static void *ftrp(void *blockp);
static void *next_blkp(void *blockp);
static void *prev_blkp(void *blockp);

static void write_block(void *blockp, size_t size, bool allocated);
static void *extend_heap(size_t size);
static void *coalesce(void *blockp);
static void *find_fit(size_t adjusted_size);
static void place(void *blockp, size_t adjusted_size);

static bool aligned(const void* p);
static bool in_heap(const void* p);

static int class_index(size_t size);
static void *next_freep(void *blockp);
static void *prev_freep(void *blockp);
static void set_next_freep(void *blockp, void *next);
static void set_prev_freep(void *blockp, void *prev);
static void insert_free_block(void *blockp);
static void remove_free_block(void *blockp);

// Rounds size up to the allocator alignment
static size_t align(size_t size){
    return DSIZE * ((size + DSIZE - 1)/DSIZE);
}

static size_t max_size(size_t a, size_t b){
    return (a > b) ? a : b;
}

// Packs block size and allocation status into one word
static size_t pack(size_t size, bool allocated){
    return allocated ? (size | 0x1) : size;
}

static size_t get_word(const void* address){
    return *(const size_t*)address;
}

static void put_word(void *address, size_t value){
    *(size_t*)address = value;
}

static size_t get_size(const void *address){
    return get_word(address) & ~(size_t)0xF;
}

static bool get_alloc(const void *address){
    return (get_word(address) & 0x1) != 0;
}

static void *hdrp(void *blockp){
    return (char*)blockp - WSIZE;
}

static void *ftrp(void *blockp){
    return (char*)blockp + get_size(hdrp(blockp)) - DSIZE;
}

static void *next_blkp(void *blockp){
    return (char*)blockp + get_size(hdrp(blockp));
}

static void *prev_blkp(void *blockp){
   return (char*)blockp - get_size((char*)blockp - DSIZE);
}

static void write_block(void *blockp, size_t size, bool allocated) {
    put_word(hdrp(blockp), pack(size, allocated));
    put_word(ftrp(blockp), pack(size, allocated));
}

// Maps a block size to a segregated free-list class
static int class_index(size_t size){
    if (size <= 32){
        return 0;
    }
    if (size <= 48){
        return 1;
    }
    if (size <= 64){
        return 2;
    }
    if (size <= 96){
        return 3;
    }
    if (size <= 128){
        return 4;
    }
    if (size <= 192){
        return 5;
    }
    if (size <= 256){
        return 6;
    }
    if (size <= 384){
        return 7;
    }
    if (size <= 512){
        return 8;
    }
    if (size <= 768){
        return 9;
    }
    if (size <= 1024){
        return 10;
    }
    if (size <= 2048){
        return 11;
    }
    if (size <= 4096){
        return 12;
    }
    return 13;
}

static void *next_freep(void *blockp){
    return *(void**)blockp;
}

static void *prev_freep(void *blockp){
    return *(void**)((char*)blockp + WSIZE);
}

static void set_next_freep(void *blockp, void *next){
    *(void**)blockp = next;
}

static void set_prev_freep(void *blockp, void *prev){
    *(void**)((char*)blockp + WSIZE) = prev;
}

// Inserts a free block into its size-class free list
static void insert_free_block(void *blockp){
    size_t size = get_size(hdrp(blockp));
    int index = class_index(size);
    if (size <= 1024){
        void *old_head = seg_free_lists[index];
        set_prev_freep(blockp, NULL);
        set_next_freep(blockp, old_head);
        if (old_head != NULL){
            set_prev_freep(old_head, blockp);
        }
        seg_free_lists[index] = blockp;
        return;
    }
    void *current = seg_free_lists[index];
    void *previous = NULL;
    while (current != NULL && current < blockp){
        previous = current;
        current = next_freep(current);
    }
    set_prev_freep(blockp, previous);
    set_next_freep(blockp, current);
    if (previous != NULL){
        set_next_freep(previous, blockp);
    } else {
        seg_free_lists[index] = blockp;
    }
    if (current != NULL){
        set_prev_freep(current, blockp);
    }
}

// Removes a free block from its size-class free list
static void remove_free_block(void *blockp){
    int index = class_index(get_size(hdrp(blockp)));
    void *prev = prev_freep(blockp);
    void *next = next_freep(blockp);
    if (prev != NULL){
        set_next_freep(prev, next);
    } else {
        seg_free_lists[index] = next;
    }
    if (next != NULL){
        set_prev_freep(next, prev);
    }
    set_next_freep(blockp, NULL);
    set_prev_freep(blockp, NULL);
}

// Extends the heap and creates a new free block
static void *extend_heap(size_t size){
    size_t adjusted_size = align(size);
    void *blockp = mm_sbrk(adjusted_size);
    if (blockp == (void*)-1){
        return NULL;
    }
    write_block(blockp, adjusted_size, false);
    put_word(hdrp(next_blkp(blockp)), pack(0, true));
    blockp = coalesce(blockp);
    insert_free_block(blockp);
    return blockp;
}

// Merges a free block with adjacent free blocks when possible
static void *coalesce(void *blockp){
    bool prev_allocated = get_alloc(ftrp(prev_blkp(blockp)));
    bool next_allocated = get_alloc(hdrp(next_blkp(blockp)));
    size_t size = get_size(hdrp(blockp));
    if (prev_allocated && next_allocated){
        return blockp;
    }
    if (prev_allocated && !next_allocated){
        remove_free_block(next_blkp(blockp));
        size += get_size(hdrp(next_blkp(blockp)));
        write_block(blockp, size, false);
        return blockp;
    }
    if (!prev_allocated && next_allocated){
        remove_free_block(prev_blkp(blockp));
        size += get_size(hdrp(prev_blkp(blockp)));
        blockp = prev_blkp(blockp);
        write_block(blockp, size, false);
        return blockp;
    }
    remove_free_block(prev_blkp(blockp));
    remove_free_block(next_blkp(blockp));
    size += get_size(hdrp(prev_blkp(blockp))) + get_size(hdrp(next_blkp(blockp)));
    blockp = prev_blkp(blockp);
    write_block(blockp, size, false);
    return blockp;
}

// Finds a suitable free block for an adjusted allocation size
static void *find_fit(size_t adjusted_size){
    int index = class_index(adjusted_size);
    void *best_blockp = NULL;
    size_t best_size = 0;
    for (int i = index; i < NUM_CLASSES; i++){
        best_blockp = NULL;
        best_size = 0;
        for (void *blockp = seg_free_lists[i]; blockp != NULL; blockp = next_freep(blockp)){
            size_t block_size = get_size(hdrp(blockp));
            if (adjusted_size <= block_size){
                if (best_blockp == NULL || block_size < best_size){
                    best_blockp = blockp;
                    best_size = block_size;
                }
                if (block_size == adjusted_size){
                    return blockp;
                }
            }
        }
        if (best_blockp != NULL){
            return best_blockp;
        }
    }
    return NULL;
}

// Places an allocation inside a selected free block
static void place(void *blockp, size_t adjusted_size){
    size_t block_size = get_size(hdrp(blockp));
    size_t remainder = block_size - adjusted_size;
    remove_free_block(blockp);
    if (remainder >= MIN_BLOCK_SIZE){ // Split only if the leftover can hold a valid free block
        write_block(blockp, adjusted_size, true);
        write_block(next_blkp(blockp), remainder, false);
        insert_free_block(next_blkp(blockp));
        return;
    }
    write_block(blockp, block_size, true);
}

////////////////////////////
////// MAIN FUNCTIONS //////
////////////////////////////

// Initializes the allocator state and creates the initial heap
bool mm_init(void){
    void *heap_start = mm_sbrk(4 * WSIZE);
    if (heap_start == (void*)-1){
        return false;
    }
    for (int i = 0; i < NUM_CLASSES; i++){
        seg_free_lists[i] = NULL;
    }
    put_word(heap_start, 0);
    put_word((char *)heap_start + WSIZE, pack(DSIZE, true));
    put_word((char *)heap_start + (2 * WSIZE), pack(DSIZE, true));
    put_word((char *)heap_start + (3 * WSIZE), pack(0, true));
    heap_listp = (char*)heap_start + (2 * WSIZE);
    if (extend_heap(CHUNKSIZE) == NULL){
        return false;
    }
    return true;
}

// Allocates a block large enough for the requested payload
void *malloc(size_t size){
    size_t adjusted_size;
    size_t extend_size;
    void *blockp;
    if (size == 0){
        return NULL;
    }
    adjusted_size = max_size(align(size + DSIZE), MIN_BLOCK_SIZE);
    blockp = find_fit(adjusted_size);
    if (blockp != NULL){
        place(blockp, adjusted_size);
        return blockp;
    }
    extend_size = max_size(adjusted_size, CHUNKSIZE);
    blockp = extend_heap(extend_size);
    if (blockp == NULL){
        return NULL;
    }
    place(blockp, adjusted_size);
    return blockp;
}

// Frees a block and returns it to the segregated free lists
void free(void *ptr){
    size_t size;
    if (ptr == NULL){
        return;
    }
    size = get_size(hdrp(ptr));
    write_block(ptr, size, false);
    ptr = coalesce(ptr);
    insert_free_block(ptr);
}

// Resizes a block while preserving its existing payload data
void *realloc(void *oldptr, size_t size){
    void *newptr;
    void *next_block;
    void *free_blockp;
    size_t old_payload_size;
    size_t copy_size;
    size_t adjusted_size;
    size_t old_block_size;
    size_t next_block_size;
    size_t combined_size;
    size_t remainder;
    if (oldptr == NULL){
        return malloc(size);
    }
    if (size == 0){
        free(oldptr);
        return NULL;
    }
    old_block_size = get_size(hdrp(oldptr));
    adjusted_size = max_size(align(size + DSIZE), MIN_BLOCK_SIZE);
    if (adjusted_size <= old_block_size){
        remainder = old_block_size - adjusted_size;
        if (remainder >= MIN_BLOCK_SIZE){
            write_block(oldptr, adjusted_size, true);
            write_block(next_blkp(oldptr), remainder, false);
            free_blockp = coalesce(next_blkp(oldptr));
            insert_free_block(free_blockp);
        }
        return oldptr;
    }
    next_block = next_blkp(oldptr);
    if (!get_alloc(hdrp(next_block)) && get_size(hdrp(next_block)) > 0){
        next_block_size = get_size(hdrp(next_block));
        combined_size = old_block_size + next_block_size;
        if (combined_size >= adjusted_size){
            remove_free_block(next_block);
            remainder = combined_size - adjusted_size;
                if (remainder >= MIN_BLOCK_SIZE){
                    write_block(oldptr, adjusted_size, true);
                    write_block(next_blkp(oldptr), remainder, false);
                    insert_free_block(next_blkp(oldptr));
                } else {
                    write_block(oldptr, combined_size, true);
                }
            return oldptr;
        }
    }
    newptr = malloc(size);
    if (newptr == NULL){
        return NULL;
    }
    old_payload_size = old_block_size - DSIZE;
    copy_size = size < old_payload_size ? size : old_payload_size;
    memcpy(newptr, oldptr, copy_size);
    free(oldptr);
    return newptr;
}

// Allocates zero-initialized memory for an array
void *calloc(size_t nmemb, size_t size) {
    size_t bytes;
    void *ptr;
    if (nmemb != 0 && size > SIZE_MAX / nmemb) { // Prevent size_t overflow
        return NULL;
    }
    bytes = nmemb * size;
    ptr = malloc(bytes);
    if (ptr != NULL) {
        memset(ptr, 0, bytes);
    }
    return ptr;
}

static bool aligned(const void *p) {
    return ((uintptr_t)p % DSIZE) == 0;
}

static bool in_heap(const void* p){
    return p >= mm_heap_lo() && p <= mm_heap_hi();
}

// Checks heap and free-list consistency during debugging
bool mm_checkheap(int line_number){
#ifdef DEBUG
    void *blockp = heap_listp;
    size_t heap_free_count = 0;
    size_t list_free_count = 0;
    if (heap_listp == NULL) {
        dbg_printf("Line %d: heap_listp is NULL\n", line_number);
        return false;
    }
    if (!aligned(heap_listp)) {
        dbg_printf("Line %d: heap_listp is not aligned\n", line_number);
        return false;
    }
    if (get_size(hdrp(blockp)) != DSIZE || !get_alloc(hdrp(blockp))){
        dbg_printf("Line %d: bad prologue block\n", line_number);
        return false;
    }
    for (blockp = next_blkp(heap_listp); get_size(hdrp(blockp)) > 0; blockp = next_blkp(blockp)){
        size_t header_size = get_size(hdrp(blockp));
        size_t footer_size = get_size(ftrp(blockp));
        bool header_alloc = get_alloc(hdrp(blockp));
        bool footer_alloc = get_alloc(ftrp(blockp));
        if (!in_heap(hdrp(blockp)) || !in_heap(ftrp(blockp))){
            dbg_printf("Line %d: block outside heap\n", line_number);
            return false;
        }
        if (!aligned(blockp)){
            dbg_printf("Line %d: block is not aligned\n", line_number);
            return false;
        }
        if (header_size != footer_size){
            dbg_printf("Line %d: header size does not match footer size\n", line_number);
            return false;
        }

        if (header_alloc != footer_alloc){
            dbg_printf("Line %d: header alloc does not match footer alloc\n", line_number);
            return false;
        }

        if (header_size < MIN_BLOCK_SIZE){
            dbg_printf("Line %d: block size is too small\n", line_number);
            return false;
        }
        if (!header_alloc){
            heap_free_count++;
            if (!get_alloc(hdrp(next_blkp(blockp))) && get_size(hdrp(next_blkp(blockp))) > 0){
                dbg_printf("Line %d: consecutive free blocks found\n", line_number);
                return false;
            }
        }
    }
    if (get_size(hdrp(blockp)) != 0 || !get_alloc(hdrp(blockp))){
        dbg_printf("Line %d: bad epilogue block\n", line_number);
        return false;
    }
    for (int i = 0; i < NUM_CLASSES; i++){
        for (void *free_block = seg_free_lists[i]; free_block != NULL; free_block = next_freep(free_block)){
            void *next = next_freep(free_block);
            void *prev = prev_freep(free_block);
            if (!in_heap(free_block)){
                dbg_printf("Line %d: free-list block outside heap\n", line_number);
                return false;
            }
            if (get_alloc(hdrp(free_block))){
                dbg_printf("Line %d: allocated block found in free list\n", line_number);
                return false;
            }
            if (class_index(get_size(hdrp(free_block))) != i){
                dbg_printf("Line %d: free block in wrong size class\n", line_number);
                return false;
            }
            if (next != NULL && prev_freep(next) != free_block){
                dbg_printf("Line %d: inconsistent next/prev free-list links\n", line_number);
                return false;
            }
            if (prev != NULL && next_freep(prev) != free_block){
                dbg_printf("Line %d: inconsistent prev/next free-list links\n", line_number);
                return false;
            }
            list_free_count++;
        }
    }
    if (heap_free_count != list_free_count){
        dbg_printf("Line %d: heap free count does not match free-list count\n", line_number);
        return false;
    }
#endif
    return true;
}