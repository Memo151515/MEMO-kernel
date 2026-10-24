/* MNG/MEMO - a tiny ring-3 program. Everything it touches lives in the .user section,
 * the only memory ring 3 may access (kernel pages are supervisor-only). */
#include <memo/arch.h>
#include <memo/sched.h>
#include <arch/usys.h>

#define UTEXT __attribute__((section(".user.text"), used))
#define UDATA __attribute__((section(".user.data"), used))

static char umsg1[] UDATA = "[ring3] Hello from userspace! (" MEMO_SYSNAME ")\n";
static char umsg2[] UDATA = "[ring3] tick...\n";
static char umsg3[] UDATA = "[ring3] bye.\n";
static char upid[]  UDATA = "[ring3] my pid is 00\n";
static uint8_t ustack[4096] UDATA __attribute__((aligned(16)));
static volatile int ufault UDATA;

void UTEXT user_demo_main(void) {
    if (ufault) { volatile int *p = (volatile int *)0x100000; usys(SYS_WRITE, (long)umsg1, 0, 0); (void)*p; }   /* touch kernel memory */
    usys(SYS_WRITE, (long)umsg1, sizeof umsg1 - 1, 0);
    long pid = usys(SYS_GETPID, 0, 0, 0);
    upid[18] = (char)('0' + (pid / 10) % 10);
    upid[19] = (char)('0' + pid % 10);
    usys(SYS_WRITE, (long)upid, sizeof upid - 1, 0);
    for (int i = 0; i < 3; i++) {
        usys(SYS_SLEEP, 400, 0, 0);
        usys(SYS_WRITE, (long)umsg2, sizeof umsg2 - 1, 0);
    }
    usys(SYS_WRITE, (long)umsg3, sizeof umsg3 - 1, 0);
    usys(SYS_EXIT, 0, 0, 0);
    for (;;) { }
}

bool user_demo_start(bool fault) {
    ufault = fault;
    return task_create_user("userdemo", (uintptr_t)user_demo_main, (uintptr_t)ustack + sizeof ustack) != NULL;
}
