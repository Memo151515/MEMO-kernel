/* MNG/MEMO - Linux x86_64 syscall compatibility layer (SYSCALL instruction, Linux numbers).
 * Goal: run unmodified static Linux/glibc programs; dynamic linking and more calls come next. */
#include <memo/console.h>
#include <memo/arch.h>
#include <memo/sched.h>
#include <memo/proc.h>
#include <memo/rtc.h>

#define O_ACCMODE 3
#define MAP_FIXED 0x10
#define MAP_ANON  0x20
#define AT_FDCWD  (-100)

static struct file *fd_get(task_t *t, int64_t fd) {
    if (fd < 0 || fd >= MAX_FDS || !t->fds || t->fds[fd].kind == FK_FREE) return NULL;
    return &t->fds[fd];
}

static int64_t sys_write_console(task_t *t, uint64_t buf, size_t n) {
    char tmp[256]; size_t done = 0;
    while (done < n) {
        size_t c = MIN(n - done, sizeof tmp);
        if (vm_copy_in(t->mm, tmp, buf + done, c)) return done ? (int64_t)done : -EFAULT;
        console_write(tmp, c);
        done += c;
    }
    return (int64_t)n;
}

static int64_t sys_write(task_t *t, int64_t fd, uint64_t buf, size_t n) {
    struct file *f = fd_get(t, fd);
    if (!f) return -EBADF;
    if (f->kind == FK_CONSOLE) return sys_write_console(t, buf, n);
    return -EBADF;                                           /* files are read-only for now */
}

static int64_t sys_read(task_t *t, int64_t fd, uint64_t buf, size_t n) {
    struct file *f = fd_get(t, fd);
    if (!f) return -EBADF;
    if (f->kind == FK_CONSOLE) return 0;                     /* stdin: EOF for now */
    if (f->kind != FK_FILE) return -EISDIR;
    fs_load(f->node);
    if (f->pos >= f->node->size) return 0;
    size_t c = MIN(n, f->node->size - f->pos);
    if (vm_copy_out(t->mm, buf, f->node->data + f->pos, c)) return -EFAULT;
    f->pos += c;
    return (int64_t)c;
}

static int64_t sys_openat(task_t *t, uint64_t path_u, int flags) {
    char path[256];
    int rc = vm_strncpy_in(t->mm, path, path_u, sizeof path);
    if (rc < 0) return rc;
    if (flags & O_ACCMODE) return -EACCES;
    fs_node_t *n = linux_lookup(path);
    if (!n) return -ENOENT;
    for (int i = 3; i < MAX_FDS; i++)
        if (t->fds[i].kind == FK_FREE) {
            t->fds[i].kind = n->type == FS_DIR ? FK_DIR : FK_FILE; t->fds[i].node = n; t->fds[i].pos = 0;
            return i;
        }
    return -24;                                              /* EMFILE */
}

static int64_t fill_stat(task_t *t, uint64_t buf, fs_node_t *n, bool console) {
    uint8_t b[144]; memset(b, 0, sizeof b);
    uint32_t mode = console ? 020620 : (n->type == FS_DIR ? 040755 : 0100755);
    if (n && !console && n->type == FS_FILE) fs_load(n);
    *(uint64_t *)(b + 8)  = n ? (uint64_t)(uintptr_t)n & 0xFFFFFF : 1;   /* st_ino */
    *(uint64_t *)(b + 16) = 1;                                          /* st_nlink */
    *(uint32_t *)(b + 24) = mode;                                       /* st_mode */
    *(uint64_t *)(b + 40) = console ? 0x8800 : 0;                       /* st_rdev */
    *(uint64_t *)(b + 48) = (n && !console) ? n->size : 0;              /* st_size */
    *(uint64_t *)(b + 56) = 4096;                                       /* st_blksize */
    *(uint64_t *)(b + 64) = (n && !console) ? (n->size + 511) / 512 : 0;/* st_blocks */
    return vm_copy_out(t->mm, buf, b, sizeof b) ? -EFAULT : 0;
}

static int64_t sys_mmap(task_t *t, uint64_t addr, size_t len, uint64_t flags, int64_t fd, uint64_t off) {
    if (!len) return -EINVAL;
    size_t np = (len + 4095) / 4096;
    uint64_t va;
    if (flags & MAP_FIXED) {
        if (addr & 0xFFF) return -EINVAL;
        va = addr;
    } else { va = t->mm->mmap_next; t->mm->mmap_next += np * 4096 + 4096; }
    if (va + np * 4096 >= 0x0000800000000000ULL) return -ENOMEM;
    if (vm_map_zero(t->mm, va, np)) return -ENOMEM;
    if (!(flags & MAP_ANON) && fd >= 0) {                    /* private file mapping: copy the bytes */
        struct file *f = fd_get(t, fd);
        if (!f || f->kind != FK_FILE) return -EBADF;
        fs_load(f->node);
        if (off < f->node->size) {
            size_t c = MIN(len, f->node->size - off);
            if (vm_copy_out(t->mm, va, f->node->data + off, c)) return -EFAULT;
        }
    }
    return (int64_t)va;
}

static int64_t sys_brk(task_t *t, uint64_t addr) {
    struct mm *mm = t->mm;
    if (addr == 0 || addr < mm->brk_start) return (int64_t)mm->brk;
    uint64_t newend = ALIGN_UP(addr, 4096ULL), oldend = ALIGN_UP(mm->brk, 4096ULL);
    if (newend > oldend) {
        if (newend >= MMAP_BASE) return (int64_t)mm->brk;
        if (vm_map_zero(mm, oldend, (newend - oldend) / 4096)) return (int64_t)mm->brk;
    }
    mm->brk = addr;
    return (int64_t)addr;
}

static uint64_t epoch_now(void) {                            /* RTC -> unix time */
    struct rtc_time r; rtc_read(&r);
    static const int mdays[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int y = r.year;
    uint64_t days = (uint64_t)(y - 1970) * 365 + (uint64_t)((y - 1969) / 4) - (uint64_t)((y - 1901) / 100) + (uint64_t)((y - 1601) / 400);
    days += (uint64_t)mdays[(r.month - 1) % 12] + (uint64_t)(r.day - 1);
    if (r.month > 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days++;
    return days * 86400 + (uint64_t)r.hour * 3600 + (uint64_t)r.min * 60 + (uint64_t)r.sec;
}

static void fill_random(void *p, size_t n) {
    static uint64_t s = 0x2545F4914F6CDD1DULL;
    uint8_t *b = p;
    s ^= (uint64_t)timer_ticks() * 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < n; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; b[i] = (uint8_t)(s >> 24); }
}

void linux_syscall(struct regs *r) {
    task_t *t = task_current();
    uint64_t nr = r->rax, a1 = r->rdi, a2 = r->rsi, a3 = r->rdx, a4 = r->r10, a5 = r->r8, a6 = r->r9;
    int64_t ret = -ENOSYS;
    (void)a4;

    switch (nr) {
    case 0:   ret = sys_read(t, (int64_t)a1, a2, a3); break;
    case 1:   ret = sys_write(t, (int64_t)a1, a2, a3); break;
    case 2:   ret = sys_openat(t, a1, (int)a2); break;                                   /* open */
    case 257: ret = sys_openat(t, a2, (int)a3); break;                                   /* openat */
    case 3:   { struct file *f = fd_get(t, (int64_t)a1); if (!f) ret = -EBADF; else { if (a1 >= 3) f->kind = FK_FREE; ret = 0; } break; }
    case 5:   { struct file *f = fd_get(t, (int64_t)a1); ret = f ? fill_stat(t, a2, f->node, f->kind == FK_CONSOLE) : -EBADF; break; }   /* fstat */
    case 4: case 6: case 262: {                                                          /* stat lstat newfstatat */
        uint64_t pu = nr == 262 ? a2 : a1, bu = nr == 262 ? a3 : a2; char path[256];
        int rc = vm_strncpy_in(t->mm, path, pu, sizeof path);
        if (rc < 0) { ret = rc; break; }
        fs_node_t *n = fs_lookup(fs_root(), path);
        ret = n ? fill_stat(t, bu, n, false) : -ENOENT; break; }
    case 17:  {                                                                          /* pread64 */
        struct file *f = fd_get(t, (int64_t)a1);
        if (!f || f->kind != FK_FILE) { ret = -EBADF; break; }
        fs_load(f->node);
        if (a4 >= f->node->size) { ret = 0; break; }
        size_t c = MIN(a3, f->node->size - a4);
        ret = vm_copy_out(t->mm, a2, f->node->data + a4, c) ? -EFAULT : (int64_t)c;
        break; }
    case 8:   { struct file *f = fd_get(t, (int64_t)a1);                                 /* lseek */
        if (!f || f->kind != FK_FILE) { ret = -EBADF; break; }
        fs_load(f->node);
        int64_t np = a3 == 0 ? (int64_t)a2 : a3 == 1 ? (int64_t)f->pos + (int64_t)a2 : (int64_t)f->node->size + (int64_t)a2;
        if (np < 0) { ret = -EINVAL; break; }
        f->pos = (size_t)np; ret = np; break; }
    case 9:   ret = sys_mmap(t, a1, a2, a4, (int64_t)a5, a6); break;
    case 10:  ret = 0; break;                                                            /* mprotect */
    case 11:  if (a1 & 0xFFF) ret = -EINVAL; else { vm_unmap(t->mm, a1, (a2 + 4095) / 4096); ret = 0; } break;
    case 12:  ret = sys_brk(t, a1); break;
    case 13: case 14: case 131: ret = 0; break;                                          /* sigaction sigprocmask sigaltstack */
    case 16:  ret = -ENOTTY; break;                                                      /* ioctl */
    case 20:  {                                                                          /* writev */
        int64_t total = 0; ret = 0;
        for (uint64_t i = 0; i < a3; i++) {
            uint64_t iov[2];
            if (vm_copy_in(t->mm, iov, a2 + i * 16, 16)) { ret = -EFAULT; break; }
            int64_t w = sys_write(t, (int64_t)a1, iov[0], iov[1]);
            if (w < 0) { ret = total ? total : w; goto done; }
            total += w;
        }
        ret = ret ? ret : total; break; }
    case 21: case 89: case 267: ret = -ENOENT; break;                                    /* access readlink readlinkat */
    case 24:  task_yield(); ret = 0; break;
    case 28:  ret = 0; break;                                                            /* madvise */
    case 35:  { uint64_t ts[2]; if (vm_copy_in(t->mm, ts, a1, 16)) { ret = -EFAULT; break; }
              task_sleep_ms((uint32_t)(ts[0] * 1000 + ts[1] / 1000000)); ret = 0; break; }
    case 39: case 186: ret = t->pid; break;                                              /* getpid gettid */
    case 110: ret = 1; break;                                                            /* getppid */
    case 102: case 104: case 107: case 108: ret = 0; break;                              /* get[e]uid/gid */
    case 63:  {                                                                          /* uname */
        char u[65 * 6]; memset(u, 0, sizeof u);
        strlcpy(u, "Linux", 65); strlcpy(u + 65, MEMO_NODENAME, 65); strlcpy(u + 130, "6.1.0-mng-memo", 65);
        strlcpy(u + 195, "#1 " MEMO_SYSNAME " " MEMO_VERSION, 65); strlcpy(u + 260, "x86_64", 65);
        ret = vm_copy_out(t->mm, a1, u, sizeof u) ? -EFAULT : 0; break; }
    case 79:  { ret = vm_copy_out(t->mm, a1, "/", 2) ? -EFAULT : 2; break; }              /* getcwd */
    case 158: {                                                                          /* arch_prctl */
        if (a1 == 0x1002) { t->fs_base = a2; arch_set_fs_base(a2); ret = 0; }
        else if (a1 == 0x1003) ret = vm_copy_out(t->mm, a2, &t->fs_base, 8) ? -EFAULT : 0;
        else ret = -EINVAL;
        break; }
    case 202: ret = 0; break;                                                            /* futex (single threaded) */
    case 218: ret = t->pid; break;                                                       /* set_tid_address */
    case 228: {                                                                          /* clock_gettime */
        uint64_t ts[2];
        if (a1 == 0) { ts[0] = epoch_now(); ts[1] = (uptime_ms() % 1000) * 1000000ULL; }
        else { uint32_t ms = uptime_ms(); ts[0] = ms / 1000; ts[1] = (ms % 1000) * 1000000ULL; }
        ret = vm_copy_out(t->mm, a2, ts, 16) ? -EFAULT : 0; break; }
    case 60: case 231: t->exit_code = (int)(a1 & 0xFF); task_exit();
    case 273: ret = 0; break;                                                            /* set_robust_list */
    case 302: {                                                                          /* prlimit64 */
        if (a4) { uint64_t lim[2] = { a2 == 3 ? 8ULL << 20 : ~0ULL, ~0ULL }; if (vm_copy_out(t->mm, a4, lim, 16)) { ret = -EFAULT; break; } }
        ret = 0; break; }
    case 318: { uint8_t rnd[256]; size_t n = MIN(a2, sizeof rnd); fill_random(rnd, n); ret = vm_copy_out(t->mm, a1, rnd, n) ? -EFAULT : (int64_t)n; break; }
    case 334: ret = -ENOSYS; break;                                                      /* rseq */
    default: {
        static int logged;
        if (logged++ < 40) printk("[linux] %s: unimplemented syscall %llu\n", t->name, (unsigned long long)nr);
        ret = -ENOSYS; }
    }
done:
    r->rax = (uint64_t)ret;
}
