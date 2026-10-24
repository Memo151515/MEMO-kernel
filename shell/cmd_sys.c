/* MNG/MEMO shell - system / hardware commands */
#include <memo/console.h>
#include <memo/sched.h>
#include <memo/bootinfo.h>
#include <memo/mm.h>
#include <memo/pci.h>
#include <memo/block.h>
#include <memo/arch.h>
#include <memo/input.h>
#include "shell.h"

static int cmd_free(int argc, char **argv) {
    (void)argc; (void)argv;
    size_t total, used, blocks; kheap_stats(&total, &used, &blocks);
    uintptr_t lo, hi; mm_heap_range(&lo, &hi);
    sh_printf("RAM (E820 usable): %u KiB\n", (unsigned)(mm_total_ram() / 1024));
    sh_printf("Kernel heap      : %p - %p\n", (void *)lo, (void *)hi);
    sh_printf("  total %u KiB, used %u KiB, free %u KiB, %u blocks\n",
              (unsigned)(total / 1024), (unsigned)(used / 1024), (unsigned)((total - used) / 1024), (unsigned)blocks);
    return 0;
}
SHELL_CMD(free, "", "memory usage", cmd_free);

static int cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    struct task_info t[32]; int n = task_list(t, 32);
    static const char *const st[] = { "ready", "running", "sleeping", "zombie" };
    sh_puts("  PID  STATE     RUNS    MODE    NAME\n");
    for (int i = 0; i < n; i++)
        sh_printf("%5u  %-8s %6u  %-6s  %s\n", t[i].pid, st[t[i].state], t[i].runs, t[i].user ? "user" : "kernel", t[i].name);
    return 0;
}
SHELL_CMD(ps, "", "list tasks", cmd_ps);

static int cmd_kill(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: kill <pid>\n"); return 1; }
    bool ok; long pid = kstrtol(argv[1], 10, &ok);
    if (!ok || pid < 0) { sh_puts("kill: bad pid\n"); return 1; }
    int rc = task_kill((uint32_t)pid);
    if (rc) { sh_printf("kill: %s\n", rc == -EINVAL ? "operation not permitted" : strerror_k(rc)); return 1; }
    return 0;
}
SHELL_CMD(kill, "<pid>", "terminate a task", cmd_kill);

static volatile uint32_t worker_counter;
static void worker(void) { for (;;) { worker_counter++; task_sleep_ms(500); } }
static int cmd_spawn(int argc, char **argv) {
    (void)argc; (void)argv;
    task_t *t = task_create("worker", worker);
    if (!t) { sh_puts("spawn: out of memory\n"); return 1; }
    sh_printf("started worker, pid %u (use 'ps' / 'kill %u')\n", t->pid, t->pid);
    return 0;
}
SHELL_CMD(spawn, "", "start a background demo thread", cmd_spawn);

#ifdef __x86_64__
static int cmd_usertest(int argc, char **argv) { (void)argc; (void)argv; sh_puts("x86_64: run real programs with 'exec <elf>'\n"); return 0; }
#else
static int cmd_usertest(int argc, char **argv) {
    bool fault = argc > 1 && !strcmp(argv[1], "fault");
    if (!user_demo_start(fault)) { sh_puts("usertest: cannot start\n"); return 1; }
    sh_puts("ring-3 program started (output comes from userspace via int 0x80)\n");
    return 0;
}
#endif
SHELL_CMD(usertest, "[fault]", "run the ring-3 demo (fault: try to read kernel memory)", cmd_usertest);

static int cmd_lsmem(int argc, char **argv) {
    (void)argc; (void)argv;
    int n; const struct mmap_entry *m = bootinfo_mmap(&n);
    if (!n) { sh_puts("no BIOS memory map (not booted by MBoot)\n"); return 1; }
    static const char *const names[] = { "?", "usable", "reserved", "ACPI reclaim", "ACPI NVS", "bad" };
    for (int i = 0; i < n; i++)
        sh_printf("%016llx - %016llx  %-12s (%llu KiB)\n", (unsigned long long)m[i].base, (unsigned long long)(m[i].base + m[i].len - 1),
                  m[i].type < 6 ? names[m[i].type] : "?", (unsigned long long)(m[i].len / 1024));
    return 0;
}
SHELL_CMD(lsmem, "", "BIOS (E820) memory map", cmd_lsmem);

static int cmd_lspci(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < pci_count(); i++) {
        const struct pci_dev *d = pci_get(i);
        sh_printf("%02x:%02x.%x %04x:%04x  %-20s %s\n", d->bus, d->dev, d->fn, d->vendor, d->device,
                  pci_class_name(d->class_code, d->subclass), pci_vendor_name(d->vendor));
    }
    return 0;
}
SHELL_CMD(lspci, "", "list PCI devices", cmd_lspci);

static int cmd_cpuinfo(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t a, b, c, d; char vendor[13], brand[49];
    cpuid(0, 0, &a, &b, &c, &d);
    memcpy(vendor, &b, 4); memcpy(vendor + 4, &d, 4); memcpy(vendor + 8, &c, 4); vendor[12] = 0;
    sh_printf("vendor   : %s\n", vendor);
    cpuid(0x80000000, 0, &a, &b, &c, &d);
    if (a >= 0x80000004) {
        for (uint32_t i = 0; i < 3; i++) { cpuid(0x80000002 + i, 0, &a, &b, &c, &d); memcpy(brand + i * 16, &a, 4); memcpy(brand + i * 16 + 4, &b, 4); memcpy(brand + i * 16 + 8, &c, 4); memcpy(brand + i * 16 + 12, &d, 4); }
        brand[48] = 0; const char *s = brand; while (*s == ' ') s++;
        sh_printf("model    : %s\n", s);
    }
    cpuid(1, 0, &a, &b, &c, &d);
    sh_printf("family   : %u  model %u  stepping %u\n", (a >> 8) & 0xF, ((a >> 4) & 0xF) | (((a >> 16) & 0xF) << 4), a & 0xF);
    sh_puts("features :");
    static const struct { char r; uint8_t bit; const char *n; } ft[] = {
        {'d',0,"fpu"},{'d',4,"tsc"},{'d',5,"msr"},{'d',6,"pae"},{'d',9,"apic"},{'d',25,"sse"},
        {'d',26,"sse2"},{'c',0,"sse3"},{'c',28,"avx"} };
    for (size_t i = 0; i < ARRAY_SIZE(ft); i++)
        if ((ft[i].r == 'd' ? d : c) & (1u << ft[i].bit)) sh_printf(" %s", ft[i].n);
    cpuid(0x80000001, 0, &a, &b, &c, &d);
    if (d & (1 << 29)) sh_puts(" lm");
    sh_printf("\nrunning  : %s kernel\n", arch_name);
    return 0;
}
SHELL_CMD(cpuinfo, "", "CPU information", cmd_cpuinfo);

static int cmd_dmesg(int argc, char **argv) {
    (void)argc; (void)argv;
    static char buf[16384];
    size_t n = klog_copy(buf, sizeof buf);
    sh_write(buf, n);
    return 0;
}
SHELL_CMD(dmesg, "", "kernel log", cmd_dmesg);

static int cmd_fbinfo(int argc, char **argv) {
    (void)argc; (void)argv;
    const struct boot_info *bi = bootinfo();
    sh_printf("console  : %s (%dx%d chars)\n", console_mode_name(), console_cols(), console_rows());
    if (bi && (bi->flags & BI_FB))
        sh_printf("display  : %ux%u, %u bpp, pitch %u, VBE mode %#x, LFB at %#x\n", bi->fb_width, bi->fb_height, bi->fb_bpp, bi->fb_pitch, bi->vbe_mode, bi->fb_addr);
    else sh_puts("display  : VGA text mode 3 (no VBE framebuffer)\n");
    return 0;
}
SHELL_CMD(fbinfo, "", "display / framebuffer information", cmd_fbinfo);

static int cmd_lsdrv(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < driver_count(); i++) {
        const char *n, *d, *s; driver_get(i, &n, &d, &s);
        sh_printf("  %-10s %-8s %s\n", n, s, d);
    }
    return 0;
}
SHELL_CMD(lsdrv, "", "list registered drivers", cmd_lsdrv);

static int cmd_lsblk(int argc, char **argv) {
    (void)argc; (void)argv;
    static const char *const nm[] = { "hda", "hdb", "hdc", "hdd" };
    bool any = false;
    for (int i = 0; i < ata_count(); i++) {
        char model[41]; uint32_t sec = 0; bool present;
        ata_info(i, model, sizeof model, &sec, &present);
        if (present) { sh_printf("%s  %u sectors (%u MiB)  %s\n", nm[i], sec, sec / 2048, model); any = true; }
    }
    if (!any) sh_puts("no ATA disks found\n");
    return 0;
}
SHELL_CMD(lsblk, "", "list ATA disks", cmd_lsblk);

static int cmd_blkread(int argc, char **argv) {
    if (argc < 3) { sh_puts("usage: blkread <0-3> <lba>\n"); return 1; }
    bool ok1, ok2; long d = kstrtol(argv[1], 10, &ok1), lba = kstrtol(argv[2], 0, &ok2);
    if (!ok1 || !ok2 || lba < 0) { sh_puts("blkread: bad number\n"); return 1; }
    static uint8_t sec[512];
    int rc = ata_read((int)d, (uint32_t)lba, 1, sec);
    if (rc) { sh_printf("blkread: %s\n", strerror_k(rc)); return 1; }
    for (int o = 0; o < 512; o += 16) {
        sh_printf("%04x  ", o);
        for (int i = 0; i < 16; i++) sh_printf("%02x ", sec[o + i]);
        sh_puts(" |");
        for (int i = 0; i < 16; i++) { char c = (char)sec[o + i]; sh_printf("%c", (c >= 32 && c < 127) ? c : '.'); }
        sh_puts("|\n");
    }
    return 0;
}
SHELL_CMD(blkread, "<disk> <lba>", "hex dump one disk sector", cmd_blkread);

static int cmd_loadkeys(int argc, char **argv) {
    if (argc < 2 || !strcmp(argv[1], "-l")) {
        sh_printf("current layout: %s\navailable layouts:\n", kbd_layout());
        for (int i = 0; i < kbd_layout_count(); i++) {
            const char *n, *d; kbd_layout_info(i, &n, &d);
            sh_printf("  %-8s %s\n", n, d);
        }
        sh_puts("usage: loadkeys <layout>\n");
        return 0;
    }
    if (kbd_set_layout(argv[1])) { sh_printf("loadkeys: unknown layout '%s' (try 'loadkeys -l')\n", argv[1]); return 1; }
    sh_printf("keyboard layout: %s\n", kbd_layout());
    return 0;
}
SHELL_CMD(loadkeys, "[-l | layout]", "list or change the keyboard layout (us uk de fr es it tr dvorak colemak)", cmd_loadkeys);
