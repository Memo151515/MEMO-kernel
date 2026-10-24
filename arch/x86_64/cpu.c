/* MNG/MEMO - x86_64: GDT, TSS, IDT, task stack setup */
#include <memo/console.h>
#include <memo/arch.h>
#include <memo/sched.h>
#include <memo/proc.h>

const char arch_name[] = "x86_64";
static void enable_sse_and_syscall(void);

struct gdt_ptr  { uint16_t limit; uint64_t base; } __packed;
struct idt_entry { uint16_t off_lo, sel; uint8_t ist, flags; uint16_t off_mid; uint32_t off_hi, zero; } __packed;
struct tss64 {
    uint32_t r0; uint64_t rsp0, rsp1, rsp2, r1, ist[7], r2; uint16_t r3, iomap;
} __packed;

static uint64_t gdt[7];                      /* null, kcode, kdata, ucode, udata, tss(2) */
static struct idt_entry idt[256];
static struct tss64 tss;
extern uint64_t isr_stub_table[48];
extern uint64_t isr_stub_128;

static void idt_set(int i, uint64_t h, uint8_t flags) {
    idt[i].off_lo = h & 0xFFFF; idt[i].off_mid = (h >> 16) & 0xFFFF; idt[i].off_hi = (uint32_t)(h >> 32);
    idt[i].sel = 0x08; idt[i].ist = 0; idt[i].flags = flags; idt[i].zero = 0;
}

void arch_early_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFULL;          /* 0x08 kernel code (L=1) */
    gdt[2] = 0x00CF92000000FFFFULL;          /* 0x10 kernel data       */
    gdt[3] = 0x00CFF2000000FFFFULL;          /* 0x18 user data   (SYSRET layout: ss = star+8)  */
    gdt[4] = 0x00AFFA000000FFFFULL;          /* 0x20 user code64 (SYSRET layout: cs = star+16) */
    memset(&tss, 0, sizeof tss);
    tss.rsp0 = (uint64_t)arch_boot_stack_top();
    tss.iomap = sizeof tss;
    uint64_t base = (uint64_t)(uintptr_t)&tss, lim = sizeof tss - 1;
    gdt[5] = (lim & 0xFFFF) | ((base & 0xFFFFFF) << 16) | (0x89ULL << 40) | (((lim >> 16) & 0xF) << 48) | (((base >> 24) & 0xFF) << 56);
    gdt[6] = base >> 32;                     /* 0x28 TSS (16 bytes) */

    struct gdt_ptr gp = { sizeof gdt - 1, (uint64_t)(uintptr_t)gdt };
    __asm__ volatile(
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t mov %%ax, %%es\n\t mov %%ax, %%fs\n\t mov %%ax, %%gs\n\t mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        "mov $0x28, %%ax\n\t"
        "ltr %%ax"
        : : "m"(gp) : "rax", "memory");

    for (int i = 0; i < 48; i++) idt_set(i, isr_stub_table[i], 0x8E);
    idt_set(0x80, isr_stub_128, 0xEE);
    struct gdt_ptr ip = { sizeof idt - 1, (uint64_t)(uintptr_t)idt };
    __asm__ volatile("lidt %0" : : "m"(ip));
    enable_sse_and_syscall();
}

uintptr_t syscall_kstack;                    /* used by syscall_entry (stubs.asm) */
extern void syscall_entry(void);

uintptr_t arch_boot_stack_top(void) { return (uintptr_t)PHYS_TO_VIRT((uint64_t)(uintptr_t)boot_stack_top); }
void arch_set_kernel_stack(uintptr_t top) { tss.rsp0 = top; syscall_kstack = top; }

static void wrmsr(uint32_t msr, uint64_t v) { __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32))); }
static uint64_t rdmsr(uint32_t msr) { uint32_t a, d; __asm__ volatile("rdmsr" : "=a"(a), "=d"(d) : "c"(msr)); return ((uint64_t)d << 32) | a; }
void arch_set_fs_base(uint64_t v) { wrmsr(0xC0000100, v); }

/* user programs use SSE (glibc does): enable it. Kernel code never touches xmm registers. */
static void enable_sse_and_syscall(void) {
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); cr0 |= (1ULL << 1);                 /* EM=0, MP=1 */
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10);                       /* OSFXSR | OSXMMEXCPT */
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    wrmsr(0xC0000080, rdmsr(0xC0000080) | 1);                /* EFER.SCE */
    wrmsr(0xC0000081, (0x10ULL << 48) | (0x08ULL << 32));    /* STAR: sysret base 0x10, syscall cs 0x08 */
    wrmsr(0xC0000082, (uint64_t)(uintptr_t)syscall_entry);   /* LSTAR */
    wrmsr(0xC0000084, 0x600);                                /* SFMASK: clear IF and DF */
}

uintptr_t arch_task_prepare_stack(uintptr_t top, void (*entry)(void), void (*exit_fn)(void)) {
    uint64_t *sp = (uint64_t *)(top & ~0xFUL);
    *--sp = (uint64_t)(uintptr_t)exit_fn;
    *--sp = (uint64_t)(uintptr_t)entry;      /* popped by "ret"; rsp then = 8 mod 16 like a call */
    *--sp = 0x202;                           /* rflags */
    for (int i = 0; i < 6; i++) *--sp = 0;   /* rbp rbx r12 r13 r14 r15 */
    return (uintptr_t)sp;
}

/* on every task switch: address space, FPU/SSE state, TLS base */
void arch_switch_hook(struct task *prev, struct task *next) {
    static uint64_t cur_cr3;
    if (!cur_cr3) cur_cr3 = kernel_cr3();
    if (prev->mm) __asm__ volatile("fxsave %0" : "=m"(*(char (*)[512])prev->fpu) : : "memory");
    uint64_t want = next->mm ? next->mm->cr3 : kernel_cr3();
    if (want != cur_cr3) { __asm__ volatile("mov %0, %%cr3" : : "r"(want) : "memory"); cur_cr3 = want; }
    if (next->mm) {
        __asm__ volatile("fxrstor %0" : : "m"(*(char (*)[512])next->fpu) : "memory");
        arch_set_fs_base(next->fs_base);
    }
}

bool arch_regs_from_user(const struct regs *r) { return (r->cs & 3) == 3; }

#define U(x) ((unsigned long long)(x))
void arch_dump_regs(const struct regs *r) {
    printk("  rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n", U(r->rax), U(r->rbx), U(r->rcx), U(r->rdx));
    printk("  rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx\n", U(r->rsi), U(r->rdi), U(r->rbp), U(r->rsp));
    printk("  rflags=%llx cs=%llx\n", U(r->rflags), U(r->cs));
}
