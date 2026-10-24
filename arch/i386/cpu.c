/* MNG/MEMO - i386: GDT, TSS, IDT, task stack setup */
#include <memo/console.h>
#include <memo/arch.h>

const char arch_name[] = "i386";

struct gdt_entry { uint16_t limit_lo, base_lo; uint8_t base_mid, access, gran, base_hi; } __packed;
struct gdt_ptr   { uint16_t limit; uint32_t base; } __packed;
struct idt_entry { uint16_t base_lo, sel; uint8_t zero, flags; uint16_t base_hi; } __packed;
struct tss_entry {
    uint32_t prev, esp0, ss0, esp1, ss1, esp2, ss2, cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap;
} __packed;

static struct gdt_entry gdt[6];
static struct idt_entry idt[256];
static struct tss_entry tss;
extern uint32_t isr_stub_table[48];
extern uint32_t isr_stub_128;

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_lo = base & 0xFFFF; gdt[i].base_mid = (base >> 16) & 0xFF; gdt[i].base_hi = (base >> 24) & 0xFF;
    gdt[i].limit_lo = limit & 0xFFFF; gdt[i].gran = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access = access;
}
static void idt_set(int i, uint32_t handler, uint8_t flags) {
    idt[i].base_lo = handler & 0xFFFF; idt[i].base_hi = handler >> 16;
    idt[i].sel = 0x08; idt[i].zero = 0; idt[i].flags = flags;
}

void arch_early_init(void) {
    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF);          /* 0x08 kernel code */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF);          /* 0x10 kernel data */
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF);          /* 0x18 user code   */
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF);          /* 0x20 user data   */
    memset(&tss, 0, sizeof tss);
    tss.ss0 = 0x10; tss.esp0 = (uint32_t)(uintptr_t)boot_stack_top; tss.iomap = sizeof tss;
    gdt_set(5, (uint32_t)(uintptr_t)&tss, sizeof tss - 1, 0x89, 0x00);   /* 0x28 TSS */

    struct gdt_ptr gp = { sizeof gdt - 1, (uint32_t)(uintptr_t)gdt };
    __asm__ volatile(
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t mov %%ax, %%es\n\t mov %%ax, %%fs\n\t mov %%ax, %%gs\n\t mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n"
        "1:\n\t"
        "mov $0x28, %%ax\n\t"
        "ltr %%ax"
        : : "m"(gp) : "ax", "memory");

    for (int i = 0; i < 48; i++) idt_set(i, isr_stub_table[i], 0x8E);
    idt_set(0x80, isr_stub_128, 0xEE);           /* DPL 3: callable from ring 3 */
    struct gdt_ptr ip = { sizeof idt - 1, (uint32_t)(uintptr_t)idt };
    __asm__ volatile("lidt %0" : : "m"(ip));
}

uintptr_t arch_boot_stack_top(void) { return (uintptr_t)boot_stack_top; }
void arch_set_kernel_stack(uintptr_t top) { tss.esp0 = (uint32_t)top; }

uintptr_t arch_task_prepare_stack(uintptr_t top, void (*entry)(void), void (*exit_fn)(void)) {
    uint32_t *sp = (uint32_t *)(top & ~0xFu);
    *--sp = (uint32_t)(uintptr_t)exit_fn;        /* return address if entry() returns */
    *--sp = (uint32_t)(uintptr_t)entry;          /* popped by "ret" in arch_switch_to */
    *--sp = 0x202;                               /* eflags: IF=1 */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; /* ebp ebx esi edi */
    return (uintptr_t)sp;
}

void arch_switch_hook(struct task *prev, struct task *next) { (void)prev; (void)next; }

bool arch_regs_from_user(const struct regs *r) { return (r->cs & 3) == 3; }

void arch_dump_regs(const struct regs *r) {
    printk("  eax=%08x ebx=%08x ecx=%08x edx=%08x\n", r->eax, r->ebx, r->ecx, r->edx);
    printk("  esi=%08x edi=%08x ebp=%08x eflags=%08x cs=%04x\n", r->esi, r->edi, r->ebp, r->eflags, r->cs);
}
