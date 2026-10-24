/* MNG/MEMO - ELF64 loader: static executables, PIE, and dynamic executables (PT_INTERP: the
 * dynamic linker, e.g. glibc's ld-linux-x86-64.so.2, is loaded next to the program and started first). */
#include <memo/arch.h>
#include <memo/proc.h>

struct elf64_hdr {
    uint8_t ident[16]; uint16_t type, machine; uint32_t version;
    uint64_t entry, phoff, shoff; uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} __packed;
struct elf64_phdr { uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align; } __packed;

#define PT_LOAD   1
#define PT_INTERP 3
#define ET_EXEC   2
#define ET_DYN    3
#define PIE_BASE    0x0000555555554000ULL
#define INTERP_BASE 0x00007E0000000000ULL

static const struct elf64_hdr *check_header(fs_node_t *f) {
    fs_load(f);
    if (f->type != FS_FILE || f->size < sizeof(struct elf64_hdr)) return NULL;
    const struct elf64_hdr *h = (const struct elf64_hdr *)f->data;
    if (memcmp(h->ident, "\177ELF", 4) || h->ident[4] != 2 || h->ident[5] != 1 || h->machine != 62) return NULL;
    if (h->type != ET_EXEC && h->type != ET_DYN) return NULL;
    if (h->phoff + (uint64_t)h->phnum * h->phentsize > f->size) return NULL;
    return h;
}

static const struct elf64_phdr *ph(fs_node_t *f, const struct elf64_hdr *h, int i) {
    return (const struct elf64_phdr *)(f->data + h->phoff + (size_t)i * h->phentsize);
}

/* map all PT_LOAD segments at bias; returns the end of the highest segment in *top */
static int load_segments(struct mm *mm, fs_node_t *f, const struct elf64_hdr *h, uint64_t bias, uint64_t *top, uint64_t *phdr) {
    *top = 0; *phdr = 0;
    for (int i = 0; i < h->phnum; i++) {
        const struct elf64_phdr *p = ph(f, h, i);
        if (p->type != PT_LOAD) continue;
        if (p->offset + p->filesz > f->size || p->filesz > p->memsz || bias + p->vaddr + p->memsz >= 0x0000800000000000ULL) return -ENOEXEC;
        uint64_t va = bias + p->vaddr;
        uint64_t start = va & ~0xFFFULL, end = ALIGN_UP(va + p->memsz, 4096ULL);
        int rc = vm_ensure(mm, start, (end - start) / 4096);
        if (rc) return rc;
        rc = vm_copy_out(mm, va, f->data + p->offset, p->filesz);
        if (rc) return rc;
        if (end > *top) *top = end;
        if (h->phoff >= p->offset && h->phoff < p->offset + p->filesz) *phdr = bias + p->vaddr + (h->phoff - p->offset);
    }
    return *top ? 0 : -ENOEXEC;
}

int elf_load(struct mm *mm, fs_node_t *f, struct elf_image *img) {
    const struct elf64_hdr *h = check_header(f);
    if (!h) return -ENOEXEC;
    uint64_t bias = h->type == ET_DYN ? PIE_BASE : 0;
    char interp[128]; interp[0] = 0;
    for (int i = 0; i < h->phnum; i++) {
        const struct elf64_phdr *p = ph(f, h, i);
        if (p->type == PT_INTERP && p->offset + p->filesz <= f->size && p->filesz < sizeof interp) {
            memcpy(interp, f->data + p->offset, p->filesz); interp[p->filesz] = 0;
        }
    }
    uint64_t top, phdr;
    int rc = load_segments(mm, f, h, bias, &top, &phdr);
    if (rc) return rc;
    memset(img, 0, sizeof *img);
    img->prog_entry = bias + h->entry; img->entry = img->prog_entry;
    img->phdr = phdr; img->phnum = h->phnum; img->phent = h->phentsize; img->brk = top;

    if (interp[0]) {                                         /* dynamic executable: load ld.so too */
        fs_node_t *lf = linux_lookup(interp);
        if (!lf) { printk("exec: dynamic linker %s not found\n", interp); return -ENOENT; }
        const struct elf64_hdr *lh = check_header(lf);
        if (!lh || lh->type != ET_DYN) return -ENOEXEC;
        uint64_t ltop, lphdr;
        rc = load_segments(mm, lf, lh, INTERP_BASE, &ltop, &lphdr);
        if (rc) return rc;
        img->entry = INTERP_BASE + lh->entry;
        img->interp_base = INTERP_BASE;
    }
    return 0;
}
