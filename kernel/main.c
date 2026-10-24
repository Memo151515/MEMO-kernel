/* MNG/MEMO - kernel entry (called from arch/<ARCH>/boot.asm) */
#include <memo/console.h>
#include <memo/bootinfo.h>
#include <memo/sched.h>
#include <memo/arch.h>
#include <memo/mm.h>
#include <memo/fs.h>
#include "../shell/shell.h"

static void ok(const char *what) {
    printk("[ ");
    console_set_color(C_LGREEN, C_BLACK); printk("OK"); console_set_color(C_LGRAY, C_BLACK);
    printk(" ] %s\n", what);
}

void kernel_main(void) {
    bootinfo_init();
    console_init();
    console_set_color(C_WHITE, C_BLACK);
    printk(MEMO_SYSNAME " " MEMO_VERSION " (%s)  -  kernel %s, userland %s (%s)\n", arch_name, MEMO_KERNEL_NAME, MEMO_USERLAND_NAME, MEMO_USERLAND_LONG);
    console_set_color(C_LGRAY, C_BLACK);
    if (!bootinfo_valid()) printk("warning: no MBoot boot info - using VGA text mode and default memory map\n");

    arch_early_init();     ok("GDT / TSS / IDT");
    pic_init();            ok("8259 PIC");
    mm_init();             ok("memory manager + paging");
    fs_init();             ok("RAM file system");
    sched_init();          ok("scheduler");
    do_initcalls();        ok("drivers (see 'lsdrv')");
    ext2_automount();
    printk("Memory: %u KiB usable, console: %s\n\n", (unsigned)(mm_total_ram() / 1024), console_mode_name());

    task_create("shell", shell_task);            /* pid 1 */
    sti();
    for (;;) {                                   /* pid 0: idle */
        task_reap();
        hlt();
    }
}
