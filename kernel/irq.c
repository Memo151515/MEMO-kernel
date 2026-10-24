/* MNG/MEMO - interrupt/exception dispatch (common to all architectures) */
#include <memo/console.h>
#include <memo/arch.h>
#include <memo/sched.h>

static irq_handler_t handlers[16];

void irq_install_handler(int irq, irq_handler_t h) {
    handlers[irq] = h;
    pic_unmask(irq);
}

static const char *const exc_names[32] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range exceeded",
    "Invalid opcode", "Device not available", "Double fault", "Coprocessor overrun",
    "Invalid TSS", "Segment not present", "Stack fault", "General protection fault",
    "Page fault", "Reserved", "x87 FPU error", "Alignment check", "Machine check",
    "SIMD FP exception", "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Security", "Reserved",
};

static void exception(struct regs *r) {
    uint32_t n = (uint32_t)r->int_no;
    console_set_color(C_LRED, C_BLACK);
    printk("\n[EXCEPTION %u] %s at ip=%p err=%#x", n, exc_names[n], (void *)(uintptr_t)REGS_IP(r), (unsigned)r->err_code);
    if (n == 14) printk(" cr2=%p", (void *)read_cr2());
    printk("\n");
    console_set_color(C_LGRAY, C_BLACK);
    if (arch_regs_from_user(r)) {
        printk("User task '%s' (pid %u) killed.\n", task_current()->name, task_current()->pid);
        task_exit();
    }
    arch_dump_regs(r);
    panic("unhandled %s in kernel mode", exc_names[n]);
}

void interrupt_dispatch(struct regs *r) {
    uint32_t n = (uint32_t)r->int_no;
    if (n < 32) { exception(r); return; }
    if (n < 48) {
        int irq = (int)n - 32;
        if (pic_is_spurious(irq)) return;
        pic_eoi(irq);                       /* EOI first: handlers may switch tasks */
        if (handlers[irq]) handlers[irq](r);
        return;
    }
    if (n == 0x80) syscall_dispatch(r);
}
