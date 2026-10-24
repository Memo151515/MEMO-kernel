/* MNG/MEMO - common kernel header */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <memo/version.h>
#include <arch/mem.h>

#define ARRAY_SIZE(a)     (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))
#define ALIGN_UP(x, a)    (((x) + ((a) - 1)) & ~((__typeof__(x))(a) - 1))
#define ALIGN_DOWN(x, a)  ((x) & ~((__typeof__(x))(a) - 1))
#define __packed          __attribute__((packed))
#define __noreturn        __attribute__((noreturn))
#define __used            __attribute__((used))
#define __printf(a, b)    __attribute__((format(printf, a, b)))

#define KiB (1024UL)
#define MiB (1024UL * 1024UL)
#define HZ  100                      /* timer ticks per second */

/* errno-style error codes (negative return values) */
#define ESRCH         3
#define ENOENT        2
#define ENOEXEC       8
#define EBADF         9
#define EACCES       13
#define E2BIG         7
#define EFAULT       14
#define ENOTTY       25
#define EROFS        30
#define ERANGE       34
#define ENOSYS       38
#define ENOMEM       12
#define EEXIST       17
#define ENOTDIR      20
#define EISDIR       21
#define EINVAL       22
#define ENAMETOOLONG 36
#define ENOTEMPTY    39
#define ENODEV       19
#define EIO           5
#define EAGAIN       11
const char *strerror_k(int err);

/* ---- lib/string.c ---- */
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
size_t strlcpy(char *d, const char *s, size_t n);
size_t strlcat(char *d, const char *s, size_t n);
char  *strstr(const char *h, const char *n);
long   kstrtol(const char *s, int base, bool *ok);
int    toupper(int c);
int    tolower(int c);

/* ---- lib/vsprintf.c / kernel/printk.c ---- */
int  vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int  snprintf(char *buf, size_t size, const char *fmt, ...) __printf(3, 4);
int  printk(const char *fmt, ...) __printf(1, 2);
void panic(const char *fmt, ...) __noreturn __printf(1, 2);
size_t klog_copy(char *dst, size_t max);        /* dmesg */

/* ---- mm/heap.c ---- */
void  kheap_init(uintptr_t start, uintptr_t end);
void *kmalloc(size_t n);
void *kcalloc(size_t n, size_t sz);
void *krealloc(void *p, size_t n);
void  kfree(void *p);
void  kheap_stats(size_t *total, size_t *used, size_t *nblocks);

/* ---- linker symbols ---- */
extern char __bss_start[], __bss_end[], kernel_end[];
extern char __user_start[], __user_end[];
extern char boot_stack_top[];

/* ---- initcalls (Linux-style) ---- */
typedef int (*initcall_t)(void);
#define __define_initcall(fn, lvl) \
    static initcall_t __initcall_##fn __used \
    __attribute__((section(".initcall." #lvl), aligned(sizeof(void *)))) = fn
#define core_initcall(fn)   __define_initcall(fn, 1)
#define device_initcall(fn) __define_initcall(fn, 2)
#define late_initcall(fn)   __define_initcall(fn, 3)
void do_initcalls(void);

/* ---- driver registry (shown by "lsdrv") ---- */
void driver_register(const char *name, const char *desc, const char *status);
int  driver_count(void);
void driver_get(int i, const char **name, const char **desc, const char **status);
