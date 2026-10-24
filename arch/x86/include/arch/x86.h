/* MNG/MEMO - helpers shared by i386 and x86_64 */
#pragma once
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v)  { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outl(uint16_t p, uint32_t v) { __asm__ volatile("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline uint8_t  inb(uint16_t p) { uint8_t  v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline uint16_t inw(uint16_t p) { uint16_t v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline uint32_t inl(uint16_t p) { uint32_t v; __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void io_wait(void) { outb(0x80, 0); }

static inline void cli(void) { __asm__ volatile("cli" ::: "memory"); }
static inline void sti(void) { __asm__ volatile("sti" ::: "memory"); }
static inline void hlt(void) { __asm__ volatile("hlt" ::: "memory"); }

static inline uintptr_t irq_save(void) {
    uintptr_t f;
    __asm__ volatile("pushf\n\tpop %0\n\tcli" : "=r"(f) : : "memory");
    return f;
}
static inline void irq_restore(uintptr_t f) {
    __asm__ volatile("push %0\n\tpopf" : : "r"(f) : "memory", "cc");
}

static inline uintptr_t read_cr2(void) {
    uintptr_t v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v;
}
static inline void cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(sub));
}
