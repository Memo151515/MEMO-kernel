#include "keyboard.h"
#include "irq.h"
#include "timer.h"
#include "fs.h"
#include <stdint.h>

static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;
static int cursor_pos = 160;

#define INPUT_BUFFER_SIZE 128
static char input_buffer[INPUT_BUFFER_SIZE];
static int input_len = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static void putchar_at(char c, int pos) {
    VGA_MEMORY[pos] = (uint16_t) c | (uint16_t) 0x0F00;
}

static void scroll(void) {
    for (int row = 0; row < VGA_HEIGHT - 1; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[row * VGA_WIDTH + col] = VGA_MEMORY[(row + 1) * VGA_WIDTH + col];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (uint16_t) ' ' | (uint16_t) 0x0F00;
    }

    cursor_pos = (VGA_HEIGHT - 1) * VGA_WIDTH * 2;
}

static void new_line(void) {
    cursor_pos = (cursor_pos / (VGA_WIDTH * 2) + 1) * (VGA_WIDTH * 2);

    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT * 2) {
        scroll();
    }
}

static void print_str(const char* s) {
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == '\n') {
            new_line();
        } else {
            putchar_at(s[i], cursor_pos / 2);
            cursor_pos += 2;
            if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT * 2) {
                scroll();
            }
        }
    }
}

static int str_eq(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void clear_screen_cmd(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = (uint16_t) ' ' | (uint16_t) 0x0F00;
    }
    cursor_pos = 0;
}

static void print_uint(uint32_t value) {
    char buf[16];
    int i = 0;

    if (value == 0) {
        buf[i++] = '0';
    } else {
        char tmp[16];
        int j = 0;
        while (value > 0) {
            tmp[j++] = '0' + (value % 10);
            value /= 10;
        }
        while (j > 0) {
            buf[i++] = tmp[--j];
        }
    }
    buf[i] = '\0';
    print_str(buf);
}

static void run_command(void) {
    input_buffer[input_len] = '\0';

    new_line();

    if (input_len == 0) {
        return;
    }

    if (str_eq(input_buffer, "help")) {
        print_str("Available commands: help, clear, about, uptime, ls, write <name> <text>, cat <name>\n");
    } else if (str_eq(input_buffer, "clear")) {
        clear_screen_cmd();
    } else if (str_eq(input_buffer, "about")) {
        print_str("MEMO v1.0 - written from scratch\n");
    } else if (str_eq(input_buffer, "uptime")) {
        print_str("Ticks: ");
        print_uint(timer_get_ticks());
        print_str("\n");
    } else if (str_eq(input_buffer, "ls")) {
        fs_list(print_str);
    } else if (input_len > 6 &&
               input_buffer[0] == 'w' && input_buffer[1] == 'r' && input_buffer[2] == 'i' &&
               input_buffer[3] == 't' && input_buffer[4] == 'e' && input_buffer[5] == ' ') {
        char fname[32];
        int i = 6, j = 0;
        while (input_buffer[i] != ' ' && input_buffer[i] != '\0' && j < 31) {
            fname[j++] = input_buffer[i++];
        }
        fname[j] = '\0';
        if (input_buffer[i] == ' ') i++;
        fs_write(fname, &input_buffer[i]);
        print_str("File written.\n");
    } else if (input_len > 4 &&
               input_buffer[0] == 'c' && input_buffer[1] == 'a' && input_buffer[2] == 't' &&
               input_buffer[3] == ' ') {
        const char* content = fs_read(&input_buffer[4]);
        if (content) {
            print_str(content);
            print_str("\n");
        } else {
            print_str("File not found.\n");
        }
    } else {
        print_str("Unknown command: ");
        print_str(input_buffer);
        print_str("\n");
    }
}

static void handle_enter(void) {
    run_command();
    input_len = 0;
}

static void handle_backspace(void) {
    if (input_len > 0) {
        input_len--;
        cursor_pos -= 2;
        putchar_at(' ', cursor_pos / 2);
    }
}

static void keyboard_callback(registers_t regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    if (scancode >= sizeof(scancode_ascii)) {
        return;
    }

    char c = scancode_ascii[scancode];
    if (c == 0) return;

    if (c == '\n') {
        handle_enter();
        return;
    }

    if (c == '\b') {
        handle_backspace();
        return;
    }

    if (input_len < INPUT_BUFFER_SIZE - 1) {
        input_buffer[input_len++] = c;
        putchar_at(c, cursor_pos / 2);
        cursor_pos += 2;
        if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT * 2) {
            scroll();
        }
    }
}

void keyboard_install(void) {
    irq_install_handler(1, keyboard_callback);
}