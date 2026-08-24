#include "timer.h"
#include "irq.h"
#include "task.h"

static uint32_t tick_count = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_callback(registers_t regs) {
    (void)regs;
    tick_count++;

    if (tick_count % 20 == 0) { 
        schedule();
    }
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

void timer_install(void) {
    irq_install_handler(0, timer_callback);

    uint32_t divisor = 1193180 / 100;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}