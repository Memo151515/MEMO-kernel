#include "kheap.h"

static uint32_t heap_ptr = 0;

void kheap_init(uint32_t start) {
    heap_ptr = start;
}

void* kmalloc(size_t size) {
    if (heap_ptr % 4 != 0) {
        heap_ptr += (4 - (heap_ptr % 4));
    }

    uint32_t addr = heap_ptr;
    heap_ptr += size;
    return (void*) addr;
}