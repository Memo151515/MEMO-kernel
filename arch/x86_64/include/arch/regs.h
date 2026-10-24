#pragma once
#include <stdint.h>
/* Layout built by arch/x86_64/stubs.asm */
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;                       /* pushed by CPU */
};
#define REGS_IP(r)      ((r)->rip)
#define REGS_CS(r)      ((r)->cs)
#define SYSCALL_NR(r)   ((r)->rax)
#define SYSCALL_A1(r)   ((r)->rdi)
#define SYSCALL_A2(r)   ((r)->rsi)
#define SYSCALL_A3(r)   ((r)->rdx)
#define SYSCALL_RET(r)  ((r)->rax)
