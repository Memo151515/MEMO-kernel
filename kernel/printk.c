/* MNG/MEMO - printk, kernel log ring buffer, panic */
#include <memo/console.h>
#include <memo/arch.h>

#define KLOG_SIZE 16384
static char   klog[KLOG_SIZE];
static size_t klog_head, klog_len;

static void klog_put(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        klog[(klog_head + klog_len) % KLOG_SIZE] = s[i];
        if (klog_len < KLOG_SIZE) klog_len++; else klog_head = (klog_head + 1) % KLOG_SIZE;
    }
}

size_t klog_copy(char *dst, size_t max) {
    size_t n = MIN(max, klog_len);
    for (size_t i = 0; i < n; i++) dst[i] = klog[(klog_head + klog_len - n + i) % KLOG_SIZE];
    return n;
}

int printk(const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    klog_put(buf, len);
    console_write(buf, len);
    return n;
}

void panic(const char *fmt, ...) {
    char buf[256];
    cli();
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    console_set_color(C_WHITE, C_RED);
    printk("\n*** KERNEL PANIC (" MEMO_SYSNAME " " MEMO_VERSION "): %s ***\n", buf);
    console_set_color(C_LGRAY, C_BLACK);
    printk("System halted.\n");
    for (;;) hlt();
}
