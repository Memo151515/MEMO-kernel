/* MNG/MEMO - physical memory discovery, kernel heap and (x86_64) physical frame allocator */
#include <memo/arch.h>
#include <memo/bootinfo.h>
#include <memo/mm.h>

#ifdef __x86_64__
#define RAM_CAP       (4ULL << 30)      /* direct map covers 4 GiB */
#define HEAP_MAX      (32ULL << 20)
#else
#define RAM_CAP       (128ULL << 20)    /* i386 identity-maps at most this much */
#endif

static uint64_t ram_total;               /* usable RAM reported by E820, bytes */
static uintptr_t heap_lo, heap_hi;

uint64_t mm_total_ram(void) { return ram_total; }
void mm_heap_range(uintptr_t *lo, uintptr_t *hi) { *lo = heap_lo; *hi = heap_hi; }

void mm_init(void) {
    int n; const struct mmap_entry *m = bootinfo_mmap(&n);
    uint64_t top = 0;
    uint64_t lo = ALIGN_UP(KIMAGE_TO_PHYS(kernel_end), 4096ULL);
    for (int i = 0; i < n; i++) {
        if (m[i].type != MMAP_USABLE) continue;
        ram_total += m[i].len;
        if (m[i].base <= 0x100000 && m[i].base + m[i].len > 0x100000) top = m[i].base + m[i].len;
    }
    if (top == 0) top = 16ULL << 20;                         /* no memory map: assume 16 MiB */
    if (top > RAM_CAP) top = RAM_CAP;
    if (!ram_total) ram_total = top;

    const struct boot_info *bi = bootinfo();
    uintptr_t mmio = 0; size_t mmio_size = 0;
    if (bi && (bi->flags & BI_FB)) { mmio = bi->fb_addr; mmio_size = (size_t)bi->fb_pitch * bi->fb_height; }
    arch_mm_init((uintptr_t)top, mmio, mmio_size);

    uint64_t hi = top;
#ifdef __x86_64__
    if (hi > lo + HEAP_MAX) hi = lo + HEAP_MAX;
    pmm_init(hi, top);                                       /* everything above the heap: frames */
#endif
    heap_lo = (uintptr_t)PHYS_TO_VIRT(lo);
    heap_hi = (uintptr_t)PHYS_TO_VIRT(hi);
    kheap_init(heap_lo, heap_hi);
}
