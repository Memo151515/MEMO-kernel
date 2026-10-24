#pragma once
/* syscall from ring 3: int 0x80, eax=nr ebx,ecx,edx = args */
static inline __attribute__((always_inline)) long usys(long nr, long a, long b, long c) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}
