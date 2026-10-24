/* MNG/MEMO - architecture interface (implemented in arch/<ARCH>/) */
#pragma once
#include <memo/kernel.h>
#include <arch/regs.h>
#include <arch/x86.h>

extern const char arch_name[];

uintptr_t arch_boot_stack_top(void);
void      arch_early_init(void);                       /* GDT, TSS, IDT        */
void      arch_mm_init(uintptr_t ram_limit, uintptr_t mmio, size_t mmio_size);   /* kernel page tables, MMIO */
void      arch_set_kernel_stack(uintptr_t top);
uintptr_t arch_task_prepare_stack(uintptr_t top, void (*entry)(void), void (*exit_fn)(void));
void      arch_switch_to(uintptr_t *old_sp, uintptr_t new_sp);
void      arch_map_mmio(uint64_t phys, size_t size);   /* x86_64: uncached MMIO window */
void      arch_enter_usermode(uintptr_t entry, uintptr_t user_sp) __noreturn;
struct task;
void      arch_switch_hook(struct task *prev, struct task *next);   /* cr3, FPU, TLS on task switch */
void      arch_dump_regs(const struct regs *r);
bool      arch_regs_from_user(const struct regs *r);

/* interrupts (kernel/irq.c) */
typedef void (*irq_handler_t)(struct regs *);
void interrupt_dispatch(struct regs *r);
void irq_install_handler(int irq, irq_handler_t h);
void pic_init(void);
void pic_unmask(int irq);
void pic_eoi(int irq);
bool pic_is_spurious(int irq);
void syscall_dispatch(struct regs *r);
void linux_syscall(struct regs *r);
