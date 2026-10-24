/* MNG/MEMO - 16550 UART (COM1). TX is mirrored from the console, RX feeds the keyboard queue. */
#include <memo/console.h>
#include <memo/arch.h>
#include <memo/input.h>

#define COM1 0x3F8
static bool present;

#ifndef CONFIG_SERIAL
void serial_early_init(void) { present = false; }
bool serial_present(void) { return false; }
void serial_putc(char c) { (void)c; }
#else
void serial_early_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);       /* DLAB */
    outb(COM1 + 0, 0x01);       /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);       /* 8N1 */
    outb(COM1 + 2, 0xC7);       /* FIFO */
    outb(COM1 + 4, 0x0B);
    present = inb(COM1 + 5) != 0xFF;
}
bool serial_present(void) { return present; }

void serial_putc(char c) {
    if (!present) return;
    for (int i = 0; i < 100000; i++) if (inb(COM1 + 5) & 0x20) break;
    outb(COM1, (uint8_t)c);
}

static int esc_state;
static void serial_irq(struct regs *r) {
    (void)r;
    while (inb(COM1 + 5) & 1) {
        uint8_t c = inb(COM1);
        if (esc_state == 1) { esc_state = (c == '[') ? 2 : 0; continue; }
        if (esc_state == 2) {
            esc_state = 0;
            switch (c) {
            case 'A': kbd_push(KEY_UP); break;    case 'B': kbd_push(KEY_DOWN); break;
            case 'C': kbd_push(KEY_RIGHT); break; case 'D': kbd_push(KEY_LEFT); break;
            case 'H': kbd_push(KEY_HOME); break;  case 'F': kbd_push(KEY_END); break;
            }
            continue;
        }
        if (c == 0x1B) { esc_state = 1; continue; }
        if (c == '\r') c = '\n';
        if (c == 0x7F) c = '\b';
        kbd_push(c);
    }
}

static int serial_init(void) {
    if (!present) { driver_register("serial", "16550 UART COM1", "not found"); return 0; }
    irq_install_handler(4, serial_irq);
    outb(COM1 + 1, 0x01);       /* RX interrupt */
    driver_register("serial", "16550 UART COM1 (115200 8N1)", "ok");
    return 0;
}
device_initcall(serial_init);
#endif /* CONFIG_SERIAL */
