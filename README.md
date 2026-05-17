# Custom Malloc Allocator

This project implements a dynamic memory allocator in C using a segregated free list design. The allocator manually manages heap memory by storing block metadata, splitting free blocks during allocation, coalescing adjacent free blocks during deallocation, and organizing free blocks into size based lists for faster search.

The allocator uses boundary tags with headers and footers to track block size and allocation status. Free blocks also store explicit next and previous pointers inside their payload area so they can be linked through segregated free lists.

## Heap Layout Overview

The heap is organized as a sequence of blocks surrounded by a prologue block at the beginning and an epilogue block at the end.

    [ Prologue Block ][ Heap Blocks / Free + Allocated Blocks ][ Epilogue Block ]

### Prologue Block

The prologue block is a small allocated block placed at the beginning of the heap. It simplifies boundary conditions by ensuring that the first real heap block always has a valid previous block.

    [ Prologue Header ][ Prologue Footer ]

### Heap Body

The body of the heap contains the actual allocated and free blocks used to satisfy allocation requests.

Each block contains a header and footer storing the block size and allocation status.

    [ Header ][ Payload ][ Footer ]

Allocated blocks use the payload area for user data.

    [ Header ][ User Payload ][ Footer ]

Free blocks use the payload area to store explicit free list links.

    [ Header ][ Next Free Pointer ][ Previous Free Pointer ][ Free Space ][ Footer ]

This allows free blocks to remain part of the physical heap while also being linked through segregated free lists.

### Epilogue Block

The epilogue block is a zero size allocated block placed at the end of the heap. It marks the end of the heap and simplifies heap traversal and coalescing logic.

    [ Epilogue Header ]

Together, the heap can be viewed as:

    [ Prologue ][ Block ][ Block ][ Block ][ Epilogue ]

where each normal block is either allocated or free:

    Allocated Block:
    [ Header ][ User Payload ][ Footer ]

    Free Block:
    [ Header ][ Next Free ][ Previous Free ][ Free Space ][ Footer ]

## Allocation Strategy

The allocator uses segregated free lists to group free blocks by size class. When `malloc` receives a request, the requested size is adjusted for alignment and metadata overhead. The allocator then searches the appropriate size class and larger classes for a suitable free block.

If a selected free block is larger than needed, it is split into an allocated block and a smaller remaining free block. If no suitable block is available, the heap is extended.

When a block is freed, the allocator marks it as free, attempts to coalesce it with adjacent free blocks, and inserts the resulting block into the appropriate segregated free list.

## Test Output Summary

The allocator was tested using the provided trace-driven `mdriver` benchmark suite. All tested traces completed successfully with valid heap behavior.

    Results for mm malloc:
      valid    util     ops   msecs    Kops  trace
       yes     4.5%       5     0.003   1481 ./traces/syn-example-short.rep
       yes    52.3%      20     0.004   4571 ./traces/syn-struct-short.rep
       yes    59.2%      20     0.005   4324 ./traces/syn-string-short.rep
       yes    80.6%      20     0.005   3810 ./traces/syn-array-short.rep
       yes    93.4%      20     0.005   4103 ./traces/syn-mix-short.rep
       yes    62.1%      36     0.006   6545 ./traces/ngram-fox1.rep
       yes   100.0%      42     0.010   4308 ./traces/syn-largemem-short.rep
       yes    78.4%     757     0.096   7885 ./traces/syn-mix-realloc.rep
       yes    63.0%    5748     0.498  11554 ./traces/bdd-aa4.rep
       yes    57.8%   87830     9.244   9501 ./traces/bdd-aa32.rep
       yes    57.6%   41080     4.167   9858 ./traces/bdd-ma4.rep
       yes    57.5%  115380    16.473   7004 ./traces/bdd-nq7.rep
       yes    41.1%   32540     2.743  11864 ./traces/ngram-gulliver1.rep
       yes    42.8%  127912    13.335   9593 ./traces/ngram-gulliver2.rep
       yes    40.0%   67012     6.137  10919 ./traces/ngram-moby1.rep
       yes    39.3%   94828     9.446  10038 ./traces/ngram-shake1.rep
       yes    95.9%   80000    48.416   1652 ./traces/syn-array.rep
       yes    92.1%   80000    15.399   5195 ./traces/syn-mix.rep
       yes    78.3%   80000     9.224   8673 ./traces/syn-string.rep
       yes    79.1%   80000     8.360   9570 ./traces/syn-struct.rep
       yes    56.7%   20547     1.684  12200 ./traces/cbit-abs.rep
       yes    58.6%   95276     8.882  10727 ./traces/cbit-parity.rep
       yes    58.4%   89623     8.122  11034 ./traces/cbit-satadd.rep
       yes    50.7%   50583     4.358  11606 ./traces/cbit-xyz.rep

    Average utilization = 60.6%
    Average throughput = 6898 Kops/sec

The results show that the allocator correctly handles allocation, deallocation, reallocation, heap extension, block splitting, and coalescing across a range of synthetic and program-derived traces.

Utilization measures how efficiently the allocator uses heap space. Throughput measures how many allocation operations are completed per second. This implementation balances correctness, free-block reuse, and allocation speed through segregated free lists, splitting, and coalescing.

## Main Functions

### `mm_init(void)`
Initializes the allocator before any allocation requests are handled. This function sets up the segregated free list array, creates the initial prologue and epilogue blocks, and extends the heap with an initial free block.

### `malloc(size_t size)`
Allocates a block of at least `size` bytes and returns a pointer to the payload. The request size is adjusted for alignment and allocator metadata before searching the segregated free lists for a suitable free block. If no fit is found, the heap is extended.

### `free(void *ptr)`
Releases a previously allocated block back to the allocator. The block is marked free, coalesced with adjacent free blocks when possible, and inserted into the appropriate segregated free list.

### `realloc(void *oldptr, size_t size)`
Resizes an existing allocation while preserving the original payload contents up to the smaller of the old and new sizes. The allocator attempts to reuse or expand the existing block when possible. Otherwise, it allocates a new block, copies the payload, and frees the old block.

### `calloc(size_t nmemb, size_t size)`
Allocates space for an array of `nmemb` elements of `size` bytes each and initializes the allocated memory to zero. The function checks for multiplication overflow before allocating.

---

## Heap Checker

### `mm_checkheap(int line_number)`
Validates heap and free list consistency during debugging. The checker verifies block alignment, heap bounds, header/footer consistency, free-list pointer integrity, coalescing correctness, size class placement, and agreement between free blocks in the heap and free blocks reachable from the segregated free lists.

---

## Helper Functions

### `align(size)`
Rounds `size` up to the nearest multiple of `DSIZE` for alignment.

### `max_size(a, b)`
Returns the larger of two size values.

### `pack(size, allocate)`
Combines a block size and allocation bit into a single header/footer value.

### `get_word(address)`
Reads one word from the given memory address.

### `put_word(address, value)`
Writes one word to the given memory address.

### `get_size(address)`
Extracts the block size from a header or footer address.

### `get_alloc(address)`
Extracts the allocation bit from a header or footer address.

### `hdrp(blockp)`
Returns the address of a block’s header from its payload pointer.

### `ftrp(blockp)`
Returns the address of a block’s footer from its payload pointer.

### `next_blkp(blockp)`
Returns the payload pointer of the next physical block in the heap.

### `prev_blkp(blockp)`
Returns the payload pointer of the previous physical block in the heap.

### `write_block(blockp, size, allocated)`
Writes a block’s header and footer using the given size and allocation status.

**Used by:** `malloc`, `free`, `realloc`, `extend_heap`, `coalesce`, `place`

### `extend_heap(size)`
Requests additional heap space, initializes the new memory as a free block, writes the new epilogue block, coalesces with the previous heap block if possible, and inserts the resulting block into the appropriate segregated free list.

**Used by:** `mm_init`, `malloc`

### `coalesce(blockp)`
Merges a free block with adjacent free blocks when possible, removes merged neighbors from their free lists, writes the combined free block, and returns the resulting block pointer.

**Used by:** `free`, `extend_heap`, `realloc`

### `find_fit(adjusted_size)`
Searches the segregated free lists starting at the appropriate size class and moving upward until a suitable block is found. Within each size class, the allocator uses a best-fit style search and returns an exact match immediately when available.

**Used by:** `malloc`

### `place(blockp, adjusted_size)`
Places an allocated block inside a selected free block and splits the block if enough free space remains.

**Used by:** `malloc`

### `aligned(p)`
Checks whether a pointer satisfies the allocator’s alignment requirement.

### `in_heap(p)`
Checks whether a pointer lies within the heap bounds.

---

## Free List Helper Functions

### `class_index(size)`
Maps a block size to the appropriate segregated free list size class.

**Used by:** `find_fit`, `insert_free_block`, `mm_checkheap`

### `next_freep(blockp)`
Returns the next free block pointer stored in a free block’s payload.

### `prev_freep(blockp)`
Returns the previous free block pointer stored in a free block’s payload.

### `set_next_freep(blockp, next)`
Writes the next free block pointer into a free block’s payload.

### `set_prev_freep(blockp, prev)`
Writes the previous free block pointer into a free block’s payload.

### `insert_free_block(blockp)`
Inserts a free block into the appropriate segregated free list. Smaller blocks are inserted at the front of their size class, while larger blocks are inserted in address order.

**Used by:** `free`, `extend_heap`, `place`, `realloc`

### `remove_free_block(blockp)`
Removes a free block from its segregated free list and updates neighboring free-list pointers.

**Used by:** `malloc`, `realloc`, `coalesce`, `place`