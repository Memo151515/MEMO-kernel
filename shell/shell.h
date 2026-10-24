#pragma once
#include <memo/kernel.h>
#include <memo/fs.h>

struct shell_cmd {
    const char *name, *usage, *help;
    int (*fn)(int argc, char **argv);
};
#define SHELL_CMD(cname, usage_, help_, func) \
    static const struct shell_cmd __shcmd_##cname __used \
    __attribute__((section(".shellcmd"), aligned(sizeof(void *)))) = { #cname, usage_, help_, func }

extern const struct shell_cmd __shellcmd_start[], __shellcmd_end[];

int  sh_printf(const char *fmt, ...) __printf(1, 2);
void sh_write(const char *s, size_t n);
void sh_puts(const char *s);
fs_node_t *sh_cwd(void);
void sh_set_cwd(fs_node_t *n);
void shell_task(void);
void sh_print_history(void);
