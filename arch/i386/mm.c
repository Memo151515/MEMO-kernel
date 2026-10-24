/* MNG/MEMO - i386 paging: 4 KiB pages, identity map of RAM. Kernel pages are supervisor-only;
 * only the .user section (ring-3 demo code/data) is user accessible. Framebuffer uses 4 MiB pages. */
#include <memo/arch.h>

#define P_PRESENT 0x001
#define P_RW      0x002
#define P_USER    0x004
#define P_PWT     0x008
#define P_PCD     0x010
#define P_PS      0x080
#define MAX_PT    32                         /* 32 * 4 MiB = 128 MiB */

static uint32_t pd[1024] __attribute__((aligned(4096)));
static uint32_t pt[MAX_PT][1024] __attribute__((aligned(4096)));

void arch_mm_init(uintptr_t ram_limit, uintptr_t mmio, size_t mmio_size) {
    uint32_t npt = (uint32_t)((ram_limit + (4UL << 20) - 1) >> 22);
    if (npt > MAX_PT) npt = MAX_PT;
    if (npt < 2) npt = 2;
    uintptr_t us = (uintptr_t)__user_start, ue = (uintptr_t)__user_end;

    for (uint32_t t = 0; t < npt; t++) {
        for (uint32_t p = 0; p < 1024; p++) {
            uintptr_t addr = ((uintptr_t)t * 1024 + p) * 4096;
            uint32_t fl = P_PRESENT | P_RW;
            if (addr == 0) fl = 0;                           /* NULL page: not present */
            else if (addr >= us && addr < ue) fl |= P_USER;
            pt[t][p] = (uint32_t)addr | fl;
        }
        /* PDE needs the user bit if any page below it may be user; the PTEs decide per page */
        pd[t] = (uint32_t)(uintptr_t)pt[t] | P_PRESENT | P_RW | P_USER;
    }
    if (mmio && mmio_size) {                                 /* framebuffer / MMIO: uncached 4 MiB pages */
        uint32_t first = (uint32_t)(mmio >> 22), last = (uint32_t)((mmio + mmio_size - 1) >> 22);
        for (uint32_t i = first; i <= last && i < 1024; i++)
            if (i >= npt) pd[i] = (i << 22) | P_PRESENT | P_RW | P_PS | P_PCD | P_PWT;
    }
    __asm__ volatile(
        "mov %%cr4, %%eax\n\t or $0x10, %%eax\n\t mov %%eax, %%cr4\n\t"     /* CR4.PSE */
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t or $0x80010000, %%eax\n\t mov %%eax, %%cr0"   /* PG | WP */
        : : "r"((uint32_t)(uintptr_t)pd) : "eax", "memory");
}
