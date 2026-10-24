#pragma once
#include <stdint.h>
/* x86_64: physical memory is direct-mapped at PHYS_BASE; the kernel image is linked at
 * KERNEL_VMA + its physical address. User space is the lower half (per process). */
#define KERNEL_VMA 0xFFFFFFFF80000000ULL
#define PHYS_BASE  0xFFFF800000000000ULL
#define PHYS_TO_VIRT(p)   ((void *)((uint64_t)(p) + PHYS_BASE))
#define KIMAGE_TO_PHYS(v) ((uint64_t)(uintptr_t)(v) - KERNEL_VMA)
#define VIRT_TO_PHYS(v)   ((uint64_t)(uintptr_t)(v) - PHYS_BASE)   /* direct-mapped pointers (kernel heap) */
