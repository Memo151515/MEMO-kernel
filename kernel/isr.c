#include "isr.h"
#include <stddef.h>

static uint16_t* const VGA = (uint16_t*) 0xB8000;

extern void syscall_handler(registers_t regs);

static const char* exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved"
};

void isr_handler(registers_t regs) {
    if (regs.int_no == 128) {
        syscall_handler(regs);
        return;
    }

    int row = 80 * 2;

    const char* prefix = "EXCEPTION: ";
    size_t i = 0;
    for (; prefix[i] != '\0'; i++) {
        VGA[row + i] = (uint16_t) prefix[i] | (uint16_t) 0x4F00;
    }

    const char* name = exception_messages[regs.int_no];
    size_t j = 0;
    for (; name[j] != '\0'; j++) {
        VGA[row + i + j] = (uint16_t) name[j] | (uint16_t) 0x4F00;
    }

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}