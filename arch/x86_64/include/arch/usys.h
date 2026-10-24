#pragma once
/* syscall from ring 3: int 0x80, rax=nr rdi,rsi,rdx = args */
static inline __attribute__((always_inline)) long usys(long nr, long a, long b, long c) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "D"(a), "S"(b), "d"(c) : "memory", "rcx", "r11");
    return ret;
}
