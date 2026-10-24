/* int 0x80 on x86_64 is not used (programs enter through SYSCALL, see proc/linux.c) */
#include <memo/arch.h>
#ifdef __x86_64__
void syscall_dispatch(struct regs *r) { SYSCALL_RET(r) = (uint64_t)-38; }
#endif
