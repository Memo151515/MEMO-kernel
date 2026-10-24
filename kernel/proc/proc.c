/* MNG/MEMO - process creation: ELF image, user stack (Linux ABI: argc/argv/envp/auxv), task setup */
#include <memo/arch.h>
#include <memo/sched.h>
#include <memo/proc.h>

#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_CLKTCK 17
#define AT_SECURE 23
#define AT_RANDOM 25

/* Linux programs expect their root file system at "/". The RAM file system is our root, so absolute
 * paths that do not exist there are retried below /disk (the mounted ext2/3/4 disk). */
fs_node_t *linux_lookup(const char *path) {
    fs_node_t *n = fs_lookup(fs_root(), path);
    if (n || path[0] != '/') return n;
    char alt[256];
    if (snprintf(alt, sizeof alt, "/disk%s", path) >= (int)sizeof alt) return NULL;
    return fs_lookup(fs_root(), alt);
}

static void proc_tramp(void) {
    task_t *t = task_current();
    arch_enter_usermode(t->user_entry, t->user_stack);
}

static int build_stack(struct mm *mm, const struct elf_image *img, int argc, char **argv, uint64_t *sp_out) {
    int rc = vm_map_zero(mm, USER_TOP - USER_STACK_SIZE, USER_STACK_SIZE / 4096);
    if (rc) return rc;
    uint8_t buf[2048];
    uint64_t base = USER_TOP - sizeof buf;
    size_t off = sizeof buf;
    uint64_t argp[16];
    if (argc > 15) argc = 15;
    for (int i = argc - 1; i >= 0; i--) {
        size_t l = strlen(argv[i]) + 1;
        if (l > off - 512) return -E2BIG;
        off -= l; memcpy(buf + off, argv[i], l); argp[i] = base + off;
    }
    off -= 16; off &= ~15UL;
    uint64_t seed = 0x9E3779B97F4A7C15ULL ^ ((uint64_t)timer_ticks() << 20) ^ (uint64_t)(uintptr_t)buf;
    for (int i = 0; i < 16; i++) { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; buf[off + i] = (uint8_t)(seed >> 56); }
    uint64_t random_va = base + off;

    uint64_t aux[][2] = {
        { AT_PHDR, img->phdr }, { AT_PHENT, img->phent }, { AT_PHNUM, img->phnum }, { AT_PAGESZ, 4096 },
        { AT_BASE, img->interp_base }, { AT_FLAGS, 0 }, { AT_ENTRY, img->prog_entry }, { AT_UID, 0 }, { AT_EUID, 0 }, { AT_GID, 0 }, { AT_EGID, 0 },
        { AT_CLKTCK, HZ }, { AT_SECURE, 0 }, { AT_RANDOM, random_va }, { AT_NULL, 0 },
    };
    size_t naux = ARRAY_SIZE(aux);
    size_t words = 1 + (size_t)argc + 1 + 1 + naux * 2;      /* argc, argv[], NULL, envp NULL, auxv */
    if (words * 8 + 64 > off) return -E2BIG;
    off -= words * 8; off &= ~15UL;
    uint64_t *w = (uint64_t *)(buf + off);
    size_t k = 0;
    w[k++] = (uint64_t)argc;
    for (int i = 0; i < argc; i++) w[k++] = argp[i];
    w[k++] = 0;                                              /* argv terminator */
    w[k++] = 0;                                              /* empty environment */
    for (size_t i = 0; i < naux; i++) { w[k++] = aux[i][0]; w[k++] = aux[i][1]; }
    rc = vm_copy_out(mm, base + off, buf + off, sizeof buf - off);
    if (rc) return rc;
    *sp_out = base + off;
    return 0;
}

int proc_exec(const char *path, int argc, char **argv, uint32_t *pid) {
    fs_node_t *f = linux_lookup(path);
    if (!f) return -ENOENT;
    if (f->type != FS_FILE) return -EACCES;
    struct mm *mm = mm_create();
    if (!mm) return -ENOMEM;
    struct elf_image img;
    int rc = elf_load(mm, f, &img);
    uint64_t sp = 0;
    if (!rc) rc = build_stack(mm, &img, argc, argv, &sp);
    if (rc) { mm_destroy(mm); return rc; }
    mm->brk_start = mm->brk = img.brk;

    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    task_t *t = task_alloc(base, proc_tramp);
    struct file *fds = kcalloc(MAX_FDS, sizeof(struct file));
    if (!t || !fds) { kfree(fds); mm_destroy(mm); return -ENOMEM; }
    for (int i = 0; i < 3; i++) fds[i].kind = FK_CONSOLE;
    t->mm = mm; t->fds = fds; t->user = true; t->detached = false;
    t->user_entry = img.entry; t->user_stack = sp;
    /* default FPU state: FCW=0x37F, MXCSR=0x1F80 */
    t->fpu[0] = 0x7F; t->fpu[1] = 0x03; t->fpu[24] = 0x80; t->fpu[25] = 0x1F;
    task_start(t);
    *pid = t->pid;
    return 0;
}
