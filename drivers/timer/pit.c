/* MNG/MEMO - 8253/8254 PIT, 100 Hz system tick; drives the scheduler */
#include <memo/arch.h>
#include <memo/sched.h>

static volatile uint32_t ticks;

uint32_t timer_ticks(void) { return ticks; }
uint32_t uptime_ms(void) { return ticks * (1000 / HZ); }

static void pit_irq(struct regs *r) {
    (void)r;
    ticks++;
    if ((ticks & 1) == 0) schedule();         /* 20 ms time slice */
}

static int pit_init(void) {
    uint32_t div = 1193182 / HZ;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(div & 0xFF));
    outb(0x40, (uint8_t)(div >> 8));
    irq_install_handler(0, pit_irq);
    driver_register("pit", "8254 timer, 100 Hz tick", "ok");
    return 0;
}
core_initcall(pit_init);
