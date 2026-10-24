/* MNG/MEMO - system call dispatcher (int 0x80) */
#include <memo/console.h>
#include <memo/arch.h>
#include <memo/sched.h>

/* ring-3 code may only hand the kernel pointers inside the user region */
static bool user_range_ok(uintptr_t p, size_t len) {
    uintptr_t s = (uintptr_t)__user_start, e = (uintptr_t)__user_end;
    return len <= e - s && p >= s && p <= e - len;
}

void syscall_dispatch(struct regs *r) {
    long ret = -EINVAL;
    switch (SYSCALL_NR(r)) {
    case SYS_WRITE: {
        uintptr_t buf = (uintptr_t)SYSCALL_A1(r);
        size_t len = (size_t)SYSCALL_A2(r);
        if (!user_range_ok(buf, len)) { ret = -EINVAL; break; }
        console_write((const char *)buf, len);
        ret = (long)len;
        break; }
    case SYS_EXIT:   task_exit();
    case SYS_GETPID: ret = (long)task_current()->pid; break;
    case SYS_UPTIME: ret = (long)uptime_ms(); break;
    case SYS_SLEEP:  task_sleep_ms((uint32_t)SYSCALL_A1(r)); ret = 0; break;
    case SYS_YIELD:  task_yield(); ret = 0; break;
    }
    SYSCALL_RET(r) = (uintptr_t)ret;
}
