#include "irq.h"

static isr_t irq_routines[16] = { 0 };

void irq_install_handler(int irq, isr_t handler) {
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); 
    outb(0xA1, 0x28); 
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

extern void idt_set_gate_ext(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

void irq_install(void) {
    pic_remap();

    idt_set_gate_ext(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate_ext(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate_ext(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate_ext(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate_ext(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate_ext(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate_ext(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate_ext(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate_ext(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate_ext(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate_ext(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate_ext(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate_ext(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate_ext(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate_ext(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate_ext(47, (uint32_t)irq15, 0x08, 0x8E);
}

void irq_handler(registers_t regs) {
    if (regs.int_no >= 40) {
        outb(0xA0, 0x20); 
    }
    outb(0x20, 0x20); 

    if (irq_routines[regs.int_no - 32] != 0) {
        isr_t handler = irq_routines[regs.int_no - 32];
        handler(regs);
    }
}