# Custom Malloc Allocator

A custom dynamic memory allocator written in C. This project implements `malloc`, `free`, `realloc`, and `calloc` using segregated explicit free lists, block splitting, coalescing, and heap consistency checking.

This repository focuses on the allocator implementation in `mm.c`. Testing traces, driver files, binaries, and build artifacts are intentionally excluded.

## Overview

The allocator manages heap memory manually by storing metadata in each block.

```text
[ Header ][ Payload ][ Footer ]
```

Free blocks use their payload space to store pointers for the explicit free list.

```text
[ Header ][ Prev Free Ptr ][ Next Free Ptr ][ Free Space ][ Footer ]
```

The heap also uses a prologue and epilogue block to simplify boundary handling during coalescing.

## Allocation Strategy

- Uses segregated explicit free lists to organize free blocks by size class
- Splits larger free blocks when only part of the block is needed
- Coalesces adjacent free blocks to reduce fragmentation
- Extends the heap when no suitable free block is available
- Includes a heap checker to validate allocator consistency during debugging

## Performance

The allocator was tested with a trace driven memory allocation harness.

| Metric | Result |
|---|---:|
| Correctness | Passed all tested traces |
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

Initializes the allocator before any allocation requests are handled.

This function clears the segregated free list array, creates the initial heap structure, writes the prologue and epilogue blocks, and extends the heap with an initial free block. The prologue and epilogue simplify boundary cases when blocks are coalesced.

### `malloc(size_t size)`

Allocates a block of at least `size` bytes and returns a pointer to the usable payload.

The requested size is adjusted to include allocator metadata and alignment requirements. The allocator then searches the segregated free lists for a suitable block. If a fit is found, the block is placed and split if enough unused space remains. If no fit exists, the heap is extended.

### `free(void *ptr)`

Releases a previously allocated block back to the allocator.

The block is marked as free, neighboring free blocks are merged when possible, and the resulting block is inserted into the appropriate segregated free list. This reduces external fragmentation and keeps future allocation searches efficient.

### `realloc(void *oldptr, size_t size)`

Resizes an existing allocation while preserving its payload contents.

If the block can be resized in place, the allocator reuses the existing block. Otherwise, it allocates a new block, copies the old payload up to the smaller of the old and new sizes, frees the old block, and returns the new pointer.

### `calloc(size_t nmemb, size_t size)`

Allocates memory for an array and initializes the result to zero.

The function first checks for multiplication overflow in `nmemb * size`. If the request is valid, it allocates the required memory using `malloc` and clears the allocated payload.

## Heap Checker

### `mm_checkheap(int line_number)`

Validates heap and free list consistency during debugging.

The checker verifies alignment, heap bounds, header/footer consistency, coalescing correctness, free list pointer integrity, size class placement, and agreement between free blocks in the heap and free blocks reachable from the segregated free lists.

## Core Helper Functions

### `extend_heap(size)`

Requests additional heap space, initializes it as a free block, writes a new epilogue block, coalesces with the previous block if possible, and inserts the resulting free block into the proper size class.

### `coalesce(blockp)`

Merges a newly freed block with adjacent free blocks when possible. Any neighboring free blocks are removed from their free lists before the combined block is written back and reinserted.

### `find_fit(adjusted_size)`

Searches the segregated free lists for a block large enough to satisfy an allocation request. The search starts in the matching size class and moves upward through larger classes if needed.

### `place(blockp, adjusted_size)`

Places an allocation inside a selected free block. If the remaining space is large enough to form another valid free block, the function splits the block and reinserts the remainder into the free list.

### `write_block(blockp, size, allocated)`

Writes the header and footer for a block using the given size and allocation status.

## Free List Helper Functions

### `class_index(size)`

Maps a block size to the correct segregated free list class.

### `insert_free_block(blockp)`

Inserts a free block into its appropriate size class. Smaller blocks are inserted at the front of the list, while larger blocks are kept in address order.

### `remove_free_block(blockp)`

Removes a free block from its segregated free list and updates the surrounding free list pointers.

### `next_freep(blockp)` / `prev_freep(blockp)`

Read the next and previous free block pointers stored inside a free block’s payload.

### `set_next_freep(blockp, next)` / `set_prev_freep(blockp, prev)`

Write the next and previous free block pointers inside a free block’s payload.

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