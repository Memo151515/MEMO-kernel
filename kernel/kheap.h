#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>

void kheap_init(uint32_t start);
void* kmalloc(size_t size);

#endif