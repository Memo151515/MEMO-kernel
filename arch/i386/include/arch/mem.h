#pragma once
#include <stdint.h>
/* i386: kernel and RAM are identity mapped (processes are x86_64 only for now) */
#define PHYS_TO_VIRT(p)   ((void *)(uintptr_t)(p))
#define VIRT_TO_PHYS(v)   ((uint64_t)(uintptr_t)(v))
#define KIMAGE_TO_PHYS(v) ((uint64_t)(uintptr_t)(v))
