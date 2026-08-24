#include "syscall.h"
#include "isr.h"
#include <stdint.h>

static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;
static const int VGA_WIDTH = 80;

#define SYS_WRITE 1

static int syscall_cursor = 22 * 80; 

static void sys_write(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        VGA_MEMORY[syscall_cursor++] = (uint16_t) str[i] | (uint16_t) 0x0E00;
    }
}

void syscall_handler(registers_t regs) {
    switch (regs.eax) {
        case SYS_WRITE:
            sys_write((const char*) regs.ebx);
            break;
        default:
            break;
    }
}

void syscall_install(void) {
    extern void idt_set_gate_ext(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
    extern void isr128();
    idt_set_gate_ext(0x80, (uint32_t)isr128, 0x08, 0xEE);
}