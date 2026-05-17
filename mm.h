#ifndef MM_H
#define MM_H

#include <stddef.h>
#include <stdbool.h>

#ifdef DRIVER

// Declared main functions to avoid collision with the standard C library 
extern void* mm_malloc(size_t size);
extern void  mm_free(void* ptr);
extern void* mm_realloc(void* ptr, size_t size);
extern void* mm_calloc(size_t nmemb, size_t size);

#else

// Standard C allocation functions
extern void* malloc(size_t size);
extern void  free(void* ptr);
extern void* realloc(void* ptr, size_t size);
extern void* calloc(size_t nmemb, size_t size);

#endif

// initializer
extern bool mm_init(void);

// For debugging heap invariants. Returns false if detected. 
extern bool mm_checkheap(int line_number);

#endif