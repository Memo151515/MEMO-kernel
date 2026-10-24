#pragma once
#include <memo/kernel.h>
void     mm_init(void);
uint64_t mm_total_ram(void);
void     mm_heap_range(uintptr_t *lo, uintptr_t *hi);

/* physical frame allocator (x86_64): 4 KiB frames, returns physical addresses (0 = out of memory) */
void     pmm_init(uint64_t lo, uint64_t hi);
uint64_t pmm_alloc(void);                 /* zeroed */
void     pmm_free(uint64_t phys);
uint64_t pmm_free_frames(void);
uint64_t pmm_total_frames(void);
