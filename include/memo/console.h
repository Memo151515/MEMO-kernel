#pragma once
#include <memo/kernel.h>

enum { C_BLACK, C_BLUE, C_GREEN, C_CYAN, C_RED, C_MAGENTA, C_BROWN, C_LGRAY,
       C_DGRAY, C_LBLUE, C_LGREEN, C_LCYAN, C_LRED, C_LMAGENTA, C_YELLOW, C_WHITE };

void console_init(void);
void console_putc(char c);
void console_write(const char *s, size_t n);
void console_puts(const char *s);
void console_clear(void);
void console_set_color(int fg, int bg);
void console_get_cursor(int *x, int *y);
void console_set_cursor(int x, int y);
int  console_cols(void);
int  console_rows(void);
uint32_t console_scroll_count(void);
const char *console_mode_name(void);

/* serial (drivers/serial) */
void serial_early_init(void);
void serial_putc(char c);
bool serial_present(void);
