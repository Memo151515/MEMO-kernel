/* MNG/MEMO shell - process commands (x86_64) */
#include <memo/sched.h>
#include <memo/proc.h>
#include "shell.h"

static int cmd_exec(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: exec <program> [args...]\n"); return 1; }
    uint32_t pid;
    int rc = proc_exec(argv[1], argc - 1, argv + 1, &pid);
    if (rc) { sh_printf("exec: %s: %s\n", argv[1], strerror_k(rc)); return 1; }
    int code = task_wait(pid);
    sh_printf("[process %u exited with code %d]\n", pid, code);
    return code;
}
SHELL_CMD(exec, "<program> [args]", "run a static Linux x86_64 ELF program", cmd_exec);
