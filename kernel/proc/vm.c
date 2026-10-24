/* MNG/MEMO - per-process address spaces (4-level page tables, 4 KiB pages) */
#include <memo/arch.h>
#include <memo/proc.h>
#include <memo/mm.h>

#define PTE_P  0x1ULL
#define PTE_RW 0x2ULL
#define PTE_US 0x4ULL
#define ADDR_MASK 0x000FFFFFFFFFF000ULL

static inline uint64_t *tbl(uint64_t phys) { return (uint64_t *)PHYS_TO_VIRT(phys); }

struct mm *mm_create(void) {
    struct mm *mm = kcalloc(1, sizeof *mm);
    if (!mm) return NULL;
    uint64_t p = pmm_alloc();
    if (!p) { kfree(mm); return NULL; }
    mm->cr3 = p; mm->pml4 = tbl(p);
    memcpy(&mm->pml4[256], &kernel_pml4()[256], 256 * sizeof(uint64_t));     /* share kernel half */
    mm->mmap_next = MMAP_BASE;
    return mm;
}

static void free_level(uint64_t table_phys, int level) {     /* level 4 = PML4 (user half only) */
    uint64_t *t = tbl(table_phys);
    int n = level == 4 ? 256 : 512;
    for (int i = 0; i < n; i++) {
        if (!(t[i] & PTE_P)) continue;
        uint64_t child = t[i] & ADDR_MASK;
        if (level > 1) free_level(child, level - 1); else pmm_free(child);
    }
    pmm_free(table_phys);
}

void mm_destroy(struct mm *mm) {
    if (!mm) return;
    free_level(mm->cr3, 4);
    kfree(mm);
}

/* walk to the PTE for va; create missing tables when create=true */
static uint64_t *walk(struct mm *mm, uint64_t va, bool create) {
    uint64_t *t = mm->pml4;
    for (int level = 4; level > 1; level--) {
        int idx = (int)((va >> (12 + 9 * (level - 1))) & 511);
        if (!(t[idx] & PTE_P)) {
            if (!create) return NULL;
            uint64_t p = pmm_alloc();
            if (!p) return NULL;
            t[idx] = p | PTE_P | PTE_RW | PTE_US;
        }
        t = tbl(t[idx] & ADDR_MASK);
    }
    return &t[(va >> 12) & 511];
}

int vm_map(struct mm *mm, uint64_t va, uint64_t pa) {
    if (va >= 0x0000800000000000ULL) return -EINVAL;
    uint64_t *pte = walk(mm, va, true);
    if (!pte) return -ENOMEM;
    if (*pte & PTE_P) pmm_free(*pte & ADDR_MASK); else mm->pages++;
    *pte = (pa & ADDR_MASK) | PTE_P | PTE_RW | PTE_US;
    return 0;
}

int vm_map_zero(struct mm *mm, uint64_t va, size_t npages) {
    for (size_t i = 0; i < npages; i++) {
        uint64_t *pte = walk(mm, va + i * 4096, true);
        if (!pte) return -ENOMEM;
        if (*pte & PTE_P) { memset(tbl(*pte & ADDR_MASK), 0, 4096); continue; }
        uint64_t p = pmm_alloc();
        if (!p) return -ENOMEM;
        *pte = p | PTE_P | PTE_RW | PTE_US;
        mm->pages++;
    }
    return 0;
}

int vm_ensure(struct mm *mm, uint64_t va, size_t npages) {
    for (size_t i = 0; i < npages; i++) {
        uint64_t *pte = walk(mm, va + i * 4096, true);
        if (!pte) return -ENOMEM;
        if (*pte & PTE_P) continue;
        uint64_t p = pmm_alloc();
        if (!p) return -ENOMEM;
        *pte = p | PTE_P | PTE_RW | PTE_US;
        mm->pages++;
    }
    return 0;
}

void vm_unmap(struct mm *mm, uint64_t va, size_t npages) {
    for (size_t i = 0; i < npages; i++) {
        uint64_t *pte = walk(mm, va + i * 4096, false);
        if (pte && (*pte & PTE_P)) { pmm_free(*pte & ADDR_MASK); *pte = 0; mm->pages--; }
    }
    uint64_t cr3; __asm__ volatile("mov %%cr3, %0\n\tmov %0, %%cr3" : "=r"(cr3) : : "memory");
}

uint64_t vm_translate(struct mm *mm, uint64_t va) {
    if (va >= 0x0000800000000000ULL) return 0;
    uint64_t *pte = walk(mm, va, false);
    if (!pte || !(*pte & PTE_P)) return 0;
    return (*pte & ADDR_MASK) | (va & 0xFFF);
}

int vm_copy_in(struct mm *mm, void *dst, uint64_t usrc, size_t n) {
    uint8_t *d = dst;
    while (n) {
        uint64_t pa = vm_translate(mm, usrc);
        if (!pa) return -14;                                  /* EFAULT */
        size_t chunk = MIN(n, 4096 - (usrc & 0xFFF));
        memcpy(d, PHYS_TO_VIRT(pa), chunk);
        d += chunk; usrc += chunk; n -= chunk;
    }
    return 0;
}

int vm_copy_out(struct mm *mm, uint64_t udst, const void *src, size_t n) {
    const uint8_t *s = src;
    while (n) {
        uint64_t pa = vm_translate(mm, udst);
        if (!pa) return -14;
        size_t chunk = MIN(n, 4096 - (udst & 0xFFF));
        memcpy(PHYS_TO_VIRT(pa), s, chunk);
        s += chunk; udst += chunk; n -= chunk;
    }
    return 0;
}

int vm_strncpy_in(struct mm *mm, char *dst, uint64_t usrc, size_t max) {
    for (size_t i = 0; i < max; i++) {
        if (vm_copy_in(mm, &dst[i], usrc + i, 1)) return -14;
        if (!dst[i]) return (int)i;
    }
    dst[max - 1] = 0;
    return -36;                                               /* ENAMETOOLONG */
}
