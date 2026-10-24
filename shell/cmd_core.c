/* MNG/MEMO shell - core commands */
#include <memo/console.h>
#include <memo/sched.h>
#include <memo/rtc.h>
#include <memo/arch.h>
#include "shell.h"

static int cmd_help(int argc, char **argv) {
    const struct shell_cmd *tab[96]; int n = 0;
    for (const struct shell_cmd *c = __shellcmd_start; c < __shellcmd_end && n < 96; c++) tab[n++] = c;
    for (int i = 1; i < n; i++) {                                   /* insertion sort */
        const struct shell_cmd *k = tab[i]; int j = i - 1;
        while (j >= 0 && strcmp(tab[j]->name, k->name) > 0) { tab[j + 1] = tab[j]; j--; }
        tab[j + 1] = k;
    }
    if (argc > 1) {
        for (int i = 0; i < n; i++) if (!strcmp(tab[i]->name, argv[1])) {
            sh_printf("usage: %s %s\n  %s\n", tab[i]->name, tab[i]->usage, tab[i]->help); return 0;
        }
        sh_printf("help: no such command '%s'\n", argv[1]); return 1;
    }
    sh_printf(MEMO_SYSNAME " " MEMO_VERSION " shell - %d commands ('help <cmd>' for details)\n\n", n);
    for (int i = 0; i < n; i++) sh_printf("  %-10s %s\n", tab[i]->name, tab[i]->help);
    return 0;
}
SHELL_CMD(help, "[command]", "list commands or show help for one", cmd_help);

static int cmd_clear(int argc, char **argv) { (void)argc; (void)argv; console_clear(); return 0; }
SHELL_CMD(clear, "", "clear the screen", cmd_clear);

static int cmd_about(int argc, char **argv) {
    (void)argc; (void)argv;
    sh_puts("\n  __  __ _   _  ____    ____  __ _____ __  __  ___\n"
            " |  \\/  | \\ | |/ ___|  / /  \\/  | ____|  \\/  |/ _ \\\n"
            " | |\\/| |  \\| | |  _  / /| |\\/| |  _| | |\\/| | | | |\n"
            " | |  | | |\\  | |_| |/ / | |  | | |___| |  | | |_| |\n"
            " |_|  |_|_| \\_|\\____/_/  |_|  |_|_____|_|  |_|\\___/\n\n");
    sh_printf("  " MEMO_SYSNAME " " MEMO_VERSION " (%s) - written from scratch\n", arch_name);
    sh_puts("  kernel : " MEMO_KERNEL_NAME "\n  userland: " MEMO_USERLAND_NAME " (" MEMO_USERLAND_LONG ")\n"
            "  license: MFCL (MEMO Free Code License)\n\n");
    return 0;
}
SHELL_CMD(about, "", "about this system", cmd_about);

static int cmd_version(int argc, char **argv) {
    (void)argc; (void)argv;
    sh_puts(MEMO_SYSNAME " " MEMO_VERSION "\n"); return 0;
}
SHELL_CMD(version, "", "print the version", cmd_version);

static int cmd_uname(int argc, char **argv) {
    bool all = argc > 1 && !strcmp(argv[1], "-a");
    if (argc > 1 && !strcmp(argv[1], "-m")) { sh_printf("%s\n", arch_name); return 0; }
    if (argc > 1 && !strcmp(argv[1], "-r")) { sh_puts(MEMO_VERSION "\n"); return 0; }
    if (all) sh_printf(MEMO_SYSNAME " " MEMO_NODENAME " " MEMO_VERSION " " MEMO_KERNEL_NAME " %s " MEMO_USERLAND_NAME "\n", arch_name);
    else sh_puts(MEMO_SYSNAME "\n");
    return 0;
}
SHELL_CMD(uname, "[-a|-m|-r]", "print system information", cmd_uname);

static int cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t s = uptime_ms() / 1000;
    sh_printf("up %ud %02u:%02u:%02u (%u ticks @ %d Hz)\n", s / 86400, (s / 3600) % 24, (s / 60) % 60, s % 60, timer_ticks(), HZ);
    return 0;
}
SHELL_CMD(uptime, "", "time since boot", cmd_uptime);

static int cmd_date(int argc, char **argv) {
    (void)argc; (void)argv;
    struct rtc_time t; rtc_read(&t);
    sh_printf("%04d-%02d-%02d %02d:%02d:%02d (RTC)\n", t.year, t.month, t.day, t.hour, t.min, t.sec);
    return 0;
}
SHELL_CMD(date, "", "show the RTC date and time", cmd_date);

static int cmd_echo(int argc, char **argv) {
    bool nl = true; int i = 1;
    if (argc > 1 && !strcmp(argv[1], "-n")) { nl = false; i = 2; }
    for (; i < argc; i++) { sh_puts(argv[i]); if (i + 1 < argc) sh_puts(" "); }
    if (nl) sh_puts("\n");
    return 0;
}
SHELL_CMD(echo, "[-n] text...", "print text (supports > and >> redirection)", cmd_echo);

static int cmd_hostname(int argc, char **argv) { (void)argc; (void)argv; sh_puts(MEMO_NODENAME "\n"); return 0; }
SHELL_CMD(hostname, "", "print the host name", cmd_hostname);

static int cmd_whoami(int argc, char **argv) { (void)argc; (void)argv; sh_puts("root\n"); return 0; }
SHELL_CMD(whoami, "", "print the current user", cmd_whoami);

static int cmd_history(int argc, char **argv) { (void)argc; (void)argv; sh_print_history(); return 0; }
SHELL_CMD(history, "", "show command history", cmd_history);

static int cmd_sleep(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: sleep <seconds>\n"); return 1; }
    bool ok; long s = kstrtol(argv[1], 10, &ok);
    if (!ok || s < 0 || s > 3600) { sh_puts("sleep: invalid time\n"); return 1; }
    task_sleep_ms((uint32_t)s * 1000);
    return 0;
}
SHELL_CMD(sleep, "<seconds>", "pause for some seconds", cmd_sleep);

static int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    sh_puts("Rebooting...\n");
    cli();
    for (int i = 0; i < 100000; i++) { if (!(inb(0x64) & 2)) break; }
    outb(0x64, 0xFE);                                                  /* pulse CPU reset via 8042 */
    struct { uint16_t l; uintptr_t b; } __attribute__((packed)) nul = { 0, 0 };
    __asm__ volatile("lidt %0\n\t int3" : : "m"(nul));                 /* triple fault fallback */
    for (;;) hlt();
    return 0;
}
SHELL_CMD(reboot, "", "restart the computer", cmd_reboot);

static int cmd_poweroff(int argc, char **argv) {
    (void)argc; (void)argv;
    sh_puts("Powering off...\n");
    cli();
    outw(0x604, 0x2000);        /* QEMU (new)   */
    outw(0xB004, 0x2000);       /* Bochs/old QEMU */
    outw(0x4004, 0x3400);       /* VirtualBox   */
    sh_puts("It is now safe to turn off your computer.\n");
    for (;;) hlt();
    return 0;
}
SHELL_CMD(poweroff, "", "power the machine off (QEMU/VBox/Bochs)", cmd_poweroff);
SHELL_CMD(shutdown, "", "same as poweroff", cmd_poweroff);

static int cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    sh_puts("System halted.\n"); cli(); for (;;) hlt();
    return 0;
}
SHELL_CMD(halt, "", "stop the CPU", cmd_halt);

static int cmd_panic(int argc, char **argv) { (void)argc; (void)argv; panic("manual panic from shell"); }
SHELL_CMD(panic, "", "trigger a kernel panic (test)", cmd_panic);
