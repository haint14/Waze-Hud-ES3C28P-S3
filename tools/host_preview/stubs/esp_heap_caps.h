#pragma once
#include <cstddef>
#define MALLOC_CAP_DMA 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_SPIRAM 4
void *heap_caps_malloc(size_t, unsigned);
void heap_caps_free(void *);
size_t heap_caps_get_free_size(unsigned);
