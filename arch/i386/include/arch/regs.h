#pragma once
#include <stdint.h>
/* Layout built by arch/i386/stubs.asm */
struct regs {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;   /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;                   /* pushed by CPU */
};
#define REGS_IP(r)      ((r)->eip)
#define REGS_CS(r)      ((r)->cs)
#define SYSCALL_NR(r)   ((r)->eax)
#define SYSCALL_A1(r)   ((r)->ebx)
#define SYSCALL_A2(r)   ((r)->ecx)
#define SYSCALL_A3(r)   ((r)->edx)
#define SYSCALL_RET(r)  ((r)->eax)
