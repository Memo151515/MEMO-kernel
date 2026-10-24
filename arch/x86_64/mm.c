/* MNG/MEMO - x86_64 kernel page tables. boot.asm built a direct map of 4 GiB with 2 MiB pages;
 * here we mark MMIO/framebuffer pages uncached and drop the temporary identity mapping. */
#include <memo/arch.h>

#define P_PWT 0x008
#define P_PCD 0x010

extern uint64_t boot_pml4[512], boot_pd[2048];
uint64_t kernel_cr3(void) { return (uint64_t)(uintptr_t)boot_pml4; }
uint64_t *kernel_pml4(void) { return (uint64_t *)PHYS_TO_VIRT((uint64_t)(uintptr_t)boot_pml4); }

void arch_map_mmio(uint64_t phys, size_t size) {          /* make 2 MiB pages covering the range uncached */
    uint64_t *pd = (uint64_t *)PHYS_TO_VIRT((uint64_t)(uintptr_t)boot_pd);
    uint64_t first = phys >> 21, last = (phys + size - 1) >> 21;
    for (uint64_t i = first; i <= last && i < 2048; i++) pd[i] |= P_PCD | P_PWT;
    uint64_t cr3; __asm__ volatile("mov %%cr3, %0\n\tmov %0, %%cr3" : "=r"(cr3) : : "memory");
}

void arch_mm_init(uintptr_t ram_limit, uintptr_t mmio, size_t mmio_size) {
    (void)ram_limit;
    if (mmio && mmio_size) arch_map_mmio(mmio, mmio_size);
    kernel_pml4()[0] = 0;                                   /* no more identity mapping */
    uint64_t cr3; __asm__ volatile("mov %%cr3, %0\n\tmov %0, %%cr3" : "=r"(cr3) : : "memory");
}
