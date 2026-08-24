#include "tss.h"
#include "gdt.h"

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

void tss_install(int gdt_slot, uint16_t ss0, uint32_t esp0) {
    uint8_t* p = (uint8_t*)&tss;
    for (uint32_t i = 0; i < sizeof(tss); i++) {
        p[i] = 0;
    }

    uint32_t base = (uint32_t)&tss;
    uint32_t limit = base + sizeof(tss) - 1;

    gdt_set_tss_gate(gdt_slot, base, limit, 0x89, 0x00);

    tss.ss0 = ss0;
    tss.esp0 = esp0;
    tss.iomap_base = sizeof(tss);

    uint16_t selector = gdt_slot * 8;
    __asm__ volatile ("ltr %0" : : "r"(selector));
}

void tss_set_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}