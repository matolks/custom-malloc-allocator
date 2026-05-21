# Custom Malloc Allocator

A custom dynamic memory allocator that implements `malloc`, `free`, `realloc`, and `calloc` using segregated explicit free lists, block splitting, coalescing, and heap consistency checking.

This repository focuses on the allocator implementation in `mm.c`. Testing traces, driver files, binaries, and build artifacts are intentionally excluded.

## Overview

This allocator manages heap memory manually using block metadata and segregated free lists. Each block stores its size and allocation status in a header and footer, which allows the allocator to move between neighboring blocks and coalesce free space.

```text
[ Header ][ Payload ][ Footer ]
```

Free blocks reuse their payload area to store free-list pointers.

```text
[ Header ][ Prev Free Ptr ][ Next Free Ptr ][ Free Space ][ Footer ]
```

Allocation requests are adjusted for metadata and 16-byte alignment. The allocator searches the appropriate size class for a fitting block, splits larger blocks when useful, coalesces adjacent free blocks when memory is released, and extends the heap when no fit is available.

The heap also uses prologue and epilogue blocks to simplify boundary cases during coalescing.

## Allocation Strategy

- Segregated explicit free lists organize free blocks by size class
- Best-fit search is used within size classes
- Larger free blocks are split when the remainder can form a valid block
- Adjacent free blocks are coalesced to reduce fragmentation
- The heap extends when no existing free block can satisfy a request
- A debug heap checker validates allocator consistency

## Performance

The allocator was tested using a local benchmarking and validation harness.

| Metric | Result |
|---|---:|
| Correctness | Passed local validation tests |
| Average utilization | 60.6% |
| Average throughput | 6898 Kops/sec |

## Repository Contents

```text
mm.c        Allocator implementation
mm.h        Allocator interface
README.md   Project documentation
.gitignore  Excluded files and build artifacts
```

## Main Functions

### `mm_init(void)`
Initializes the allocator, clears the free lists, creates the initial heap structure, and extends the heap with the first free block.

### `malloc(size_t size)`
Allocates a block of at least `size` bytes after adjusting the request for metadata and alignment.

### `free(void *ptr)`
Releases a block, coalesces it with adjacent free blocks when possible, and returns it to the proper free list.

### `realloc(void *oldptr, size_t size)`
Resizes an allocation while preserving existing payload data. The allocator tries to reuse or expand the current block before allocating a new one.

### `calloc(size_t nmemb, size_t size)`
Allocates space for an array and initializes the payload to zero. Includes an overflow check before computing the total size.

## Heap Checker

### `mm_checkheap(int line_number)`
Checks heap and free-list consistency during debugging. It validates alignment, heap bounds, header/footer agreement, coalescing, free-list links, size-class placement, and free-block counts.

## Core Helper Functions

### `extend_heap(size)`
Requests more heap space, creates a new free block, writes a new epilogue, coalesces if possible, and inserts the block into a free list.

### `coalesce(blockp)`
Merges a free block with adjacent free blocks when possible.

### `find_fit(adjusted_size)`
Searches the segregated free lists for a suitable block.

### `place(blockp, adjusted_size)`
Places an allocation inside a selected free block and splits the remainder if it can form a valid free block.

### `write_block(blockp, size, allocated)`
Writes the header and footer for a block.

## Free List Helper Functions

### `class_index(size)`
Maps a block size to the correct segregated free-list class.

### `insert_free_block(blockp)`
Inserts a free block into its size-class free list.

### `remove_free_block(blockp)`
Removes a free block from its size-class free list.

### `next_freep(blockp)` / `prev_freep(blockp)`
Read the next and previous free-block pointers stored in a free block’s payload.

### `set_next_freep(blockp, next)` / `set_prev_freep(blockp, prev)`
Write the next and previous free-block pointers inside a free block’s payload.

## Utility Helper Functions

These helpers keep pointer arithmetic and metadata access consistent throughout the allocator.

| Helper | Purpose |
|---|---|
| `align(size)` | Rounds a size up to the required alignment |
| `max_size(a, b)` | Returns the larger of two sizes |
| `pack(size, allocate)` | Combines block size and allocation bit |
| `get_word(address)` | Reads one word from memory |
| `put_word(address, value)` | Writes one word to memory |
| `get_size(address)` | Extracts block size from a header/footer |
| `get_alloc(address)` | Extracts allocation status from a header/footer |
| `hdrp(blockp)` | Returns a block’s header address |
| `ftrp(blockp)` | Returns a block’s footer address |
| `next_blkp(blockp)` | Returns the next physical heap block |
| `prev_blkp(blockp)` | Returns the previous physical heap block |
| `aligned(p)` | Checks whether a pointer is properly aligned |
| `in_heap(p)` | Checks whether a pointer is inside the heap |

## Concepts Demonstrated

- Dynamic memory allocation
- Explicit and segregated free lists
- Block splitting and coalescing
- Heap metadata design
- Pointer arithmetic
- Alignment
- Fragmentation and throughput tradeoffs
- Heap consistency checking