# Custom Malloc Allocator

A custom C dynamic memory allocator implementing `malloc`, `free`, `realloc`, and `calloc` with segregated explicit free lists, block splitting, coalescing, 16-byte alignment, and heap consistency checking.

This repository focuses on the allocator implementation in `mm.c`. Testing traces, driver files, binaries, and build artifacts are intentionally excluded.

## Overview

This allocator manages heap memory manually using block metadata and segregated free lists. Each block stores its size and allocation status in a header and footer, allowing the allocator to inspect neighboring blocks and coalesce adjacent free space.

```text
[ Header ][ Payload ][ Footer ]
```

Free blocks reuse their payload area to store explicit free-list pointers.

```text
[ Header ][ Prev Free Ptr ][ Next Free Ptr ][ Free Space ][ Footer ]
```

Allocation requests are adjusted for metadata and 16-byte alignment. The allocator searches the segregated free lists for a fitting block, splits larger blocks when useful, coalesces adjacent free blocks when memory is released, and extends the heap when no suitable block is available.

The heap uses prologue and epilogue blocks to simplify boundary handling during allocation and coalescing.

## Allocation Strategy

- Segregated explicit free lists organize free blocks by size class
- Best-fit search is used within size classes
- Larger free blocks are split when the remainder can form a valid block
- Adjacent free blocks are coalesced to reduce fragmentation
- The heap extends when no existing free block can satisfy a request
- A debug heap checker validates allocator consistency

## Main Functions

| Function  | Purpose                                                                                       |
| --------- | --------------------------------------------------------------------------------------------- |
| `mm_init` | Initializes the allocator, free lists, prologue block, epilogue block, and initial heap space |
| `malloc`  | Allocates an aligned block large enough for the requested payload                             |
| `free`    | Releases a block, coalesces adjacent free space, and returns the result to a free list        |
| `realloc` | Resizes an allocation while preserving existing payload data when possible                    |
| `calloc`  | Allocates zero-initialized memory and checks for multiplication overflow                      |

## Heap Checker

The allocator includes a debug heap checker that validates heap and free-list consistency during development.

The checker verifies:

- Payload alignment
- Heap boundary validity
- Header and footer agreement
- Coalescing correctness
- Free-list pointer consistency
- Size-class placement
- Free-block count agreement between the heap and free lists

## Performance

The allocator was tested using a local benchmarking and validation harness.

| Metric              |                        Result |
| ------------------- | ----------------------------: |
| Correctness         | Passed local validation tests |
| Average utilization |                         60.6% |
| Average throughput  |                 6898 Kops/sec |

## Repository Contents

```text
mm.c        Allocator implementation
mm.h        Allocator interface
README.md   Project documentation
.gitignore  Excluded files and build artifacts
```

## Concepts Demonstrated

- Dynamic memory allocation
- Explicit and segregated free lists
- Block splitting and coalescing
- Heap metadata design
- Pointer arithmetic
- Alignment
- Fragmentation and throughput tradeoffs
- Heap consistency checking
