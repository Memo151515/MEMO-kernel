/* MNG/MEMO - user processes: address spaces, ELF loading, Linux syscall compatibility (x86_64) */
#pragma once
#include <memo/kernel.h>
#include <memo/fs.h>
#include <arch/regs.h>

#define USER_TOP        0x00007FFFFFFFF000ULL
#define USER_STACK_SIZE (256 * 4096)
#define MMAP_BASE       0x00007F0000000000ULL
#define MAX_FDS         16

struct mm {
    uint64_t cr3;                 /* physical address of the PML4 */
    uint64_t *pml4;               /* virtual (direct map) */
    uint64_t brk_start, brk;
    uint64_t mmap_next;
    uint64_t pages;               /* mapped user frames */
};

struct file { uint8_t kind; fs_node_t *node; size_t pos; };   /* kind: 0 free, 1 console, 2 file, 3 dir */
enum { FK_FREE, FK_CONSOLE, FK_FILE, FK_DIR };

/* vm.c */
struct mm *mm_create(void);
void       mm_destroy(struct mm *mm);
int        vm_map(struct mm *mm, uint64_t va, uint64_t pa);
int        vm_map_zero(struct mm *mm, uint64_t va, size_t npages);
void       vm_unmap(struct mm *mm, uint64_t va, size_t npages);
uint64_t   vm_translate(struct mm *mm, uint64_t va);
int        vm_copy_in(struct mm *mm, void *dst, uint64_t usrc, size_t n);
int        vm_copy_out(struct mm *mm, uint64_t udst, const void *src, size_t n);
int        vm_strncpy_in(struct mm *mm, char *dst, uint64_t usrc, size_t max);
uint64_t   kernel_cr3(void);
uint64_t  *kernel_pml4(void);
void       arch_set_fs_base(uint64_t v);
void       arch_map_mmio(uint64_t phys, size_t size);

/* elf.c / proc.c */
struct elf_image { uint64_t entry, prog_entry, phdr, phnum, phent, brk, interp_base; };
fs_node_t *linux_lookup(const char *path);          /* absolute paths fall back to the mounted ext disk (/disk) */
int        vm_ensure(struct mm *mm, uint64_t va, size_t npages);   /* map missing pages only (keeps contents) */
int  elf_load(struct mm *mm, fs_node_t *f, struct elf_image *img);
int  proc_exec(const char *path, int argc, char **argv, uint32_t *pid);

/* linux.c */
void linux_syscall(struct regs *r);
