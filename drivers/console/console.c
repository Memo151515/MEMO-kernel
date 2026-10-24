/* MNG/MEMO - text console. Backends: VBE linear framebuffer (8x16 BIOS font) or VGA text mode. */
#include <memo/console.h>
#include <memo/bootinfo.h>
#include <memo/arch.h>
#include <memo/input.h>

static const uint32_t palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

enum { MODE_NONE, MODE_VGA, MODE_FB };
static int mode = MODE_NONE;
static int cols = 80, rows = 25, cx, cy;
static int fg = C_LGRAY, bg = C_BLACK;
static uint32_t scrolls;
static bool cursor_shown;

/* framebuffer state */
static volatile uint8_t *fb;
static uint32_t fb_pitch, fb_w, fb_h;
static uint8_t  font[4096];
#define FW 8
#define FH 16

/* VGA state */
static volatile uint16_t *vga;

static inline void fb_pixel_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; j++) {
        volatile uint32_t *p = (volatile uint32_t *)(fb + (y + j) * fb_pitch + x * 4);
        for (uint32_t i = 0; i < w; i++) p[i] = color;
    }
}

/* CP437 lacks g-breve, dotless/dotted i and s-cedilla: derive them from existing glyphs */
static void patch_turkish_glyphs(void) {
    uint8_t *g;
    /* dotless i: 'i' without its dot (rows up to the first blank row after the dot) */
    g = &font[TR_DOTLESS_I * FH]; memcpy(g, &font['i' * FH], FH);
    int j = 0; while (j < FH && !g[j]) j++;
    while (j < FH && g[j]) g[j++] = 0;
    /* dotted capital I */
    g = &font[TR_DOTTED_I * FH]; memcpy(g, &font['I' * FH], FH); g[0] = 0x18; g[1] = 0x18; if (font['I' * FH + 1]) g[1] = font['I' * FH + 1];
    /* s with cedilla */
    g = &font[TR_SCEDIL_L * FH]; memcpy(g, &font['s' * FH], FH); g[12] = 0x18; g[13] = 0x0C; g[14] = 0x78 & 0x78;
    g = &font[TR_SCEDIL_U * FH]; memcpy(g, &font['S' * FH], FH); g[13] = 0x18; g[14] = 0x0C; g[15] = 0x78;
    /* g with breve */
    g = &font[TR_GBREVE_L * FH]; memcpy(g, &font['g' * FH], FH); g[2] = 0x66; g[3] = 0x3C;
    g = &font[TR_GBREVE_U * FH]; memcpy(g, &font['G' * FH], FH); g[0] = 0x66; g[1] = 0x3C;
}

static void put_cell(int x, int y, unsigned char ch, int f, int b) {
    if (mode == MODE_VGA) {
        vga[y * cols + x] = (uint16_t)ch | (uint16_t)((b << 4 | f) << 8);
    } else if (mode == MODE_FB) {
        const uint8_t *g = &font[ch * FH];
        uint32_t fc = palette[f], bc = palette[b];
        for (int j = 0; j < FH; j++) {
            volatile uint32_t *p = (volatile uint32_t *)(fb + (uint32_t)(y * FH + j) * fb_pitch + (uint32_t)x * FW * 4);
            uint8_t bits = g[j];
            for (int i = 0; i < FW; i++) p[i] = (bits & (0x80 >> i)) ? fc : bc;
        }
    }
}

static void cursor_draw(void) {
    if (mode == MODE_FB) {
        /* underline-style block, XOR so it can be erased */
        for (int j = FH - 3; j < FH; j++) {
            volatile uint32_t *p = (volatile uint32_t *)(fb + (uint32_t)(cy * FH + j) * fb_pitch + (uint32_t)cx * FW * 4);
            for (int i = 0; i < FW; i++) p[i] ^= 0x00FFFFFF;
        }
    } else if (mode == MODE_VGA) {
        uint16_t pos = (uint16_t)(cy * cols + cx);
        outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
    }
}
static void cursor_show(void) { if (!cursor_shown) { cursor_draw(); cursor_shown = true; } }
static void cursor_hide(void) { if (cursor_shown && mode == MODE_FB) cursor_draw(); cursor_shown = false; }

static void scroll_up(void) {
    scrolls++;
    if (mode == MODE_VGA) {
        for (int i = 0; i < cols * (rows - 1); i++) vga[i] = vga[i + cols];
        for (int i = 0; i < cols; i++) vga[(rows - 1) * cols + i] = (uint16_t)' ' | (uint16_t)((bg << 4 | fg) << 8);
    } else if (mode == MODE_FB) {
        uint32_t row_bytes = fb_pitch * FH;
        uint32_t n = row_bytes * (uint32_t)(rows - 1) / 4;
        volatile uint32_t *dst = (volatile uint32_t *)fb;
        volatile uint32_t *src = (volatile uint32_t *)(fb + row_bytes);
        for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
        fb_pixel_fill(0, (uint32_t)(rows - 1) * FH, fb_w, FH, palette[bg]);
    }
}

static void newline(void) {
    cx = 0;
    if (++cy >= rows) { cy = rows - 1; scroll_up(); }
}

static void putc_raw(char c) {
    switch (c) {
    case '\n': newline(); break;
    case '\r': cx = 0; break;
    case '\b': if (cx > 0) cx--; break;
    case '\t': cx = (cx + 8) & ~7; if (cx >= cols) newline(); break;
    default:
        put_cell(cx, cy, (unsigned char)c, fg, bg);
        if (++cx >= cols) newline();
    }
}

void console_putc(char c) { console_write(&c, 1); }

void console_write(const char *s, size_t n) {
    uintptr_t fl = irq_save();
    cursor_hide();
    for (size_t i = 0; i < n; i++) {
        if (mode != MODE_NONE) putc_raw(s[i]);
        if (s[i] == '\n') serial_putc('\r');
        serial_putc(s[i]);
    }
    if (mode != MODE_NONE) cursor_show();
    irq_restore(fl);
}
void console_puts(const char *s) { console_write(s, strlen(s)); }

void console_clear(void) {
    uintptr_t fl = irq_save();
    cursor_hide();
    if (mode == MODE_VGA) {
        for (int i = 0; i < cols * rows; i++) vga[i] = (uint16_t)' ' | (uint16_t)((bg << 4 | fg) << 8);
    } else if (mode == MODE_FB) {
        fb_pixel_fill(0, 0, fb_w, fb_h, palette[bg]);
    }
    cx = cy = 0;
    cursor_show();
    irq_restore(fl);
}

void console_set_color(int f, int b) { fg = f & 15; bg = b & 15; }
void console_get_cursor(int *x, int *y) { *x = cx; *y = cy; }
void console_set_cursor(int x, int y) {
    uintptr_t fl = irq_save();
    cursor_hide();
    cx = x < 0 ? 0 : (x >= cols ? cols - 1 : x);
    cy = y < 0 ? 0 : (y >= rows ? rows - 1 : y);
    cursor_show();
    irq_restore(fl);
}
int console_cols(void) { return cols; }
int console_rows(void) { return rows; }
uint32_t console_scroll_count(void) { return scrolls; }
const char *console_mode_name(void) { return mode == MODE_FB ? "framebuffer" : mode == MODE_VGA ? "vga-text" : "none"; }

void console_init(void) {
    vga = (volatile uint16_t *)PHYS_TO_VIRT(0xB8000);
    serial_early_init();
    const struct boot_info *bi = bootinfo();
    const uint8_t *f = bootinfo_font();
    if (bi && (bi->flags & BI_FB) && f && bi->fb_bpp == 32 && bi->fb_addr) {
        memcpy(font, f, sizeof font);
        patch_turkish_glyphs();
        fb = (volatile uint8_t *)PHYS_TO_VIRT(bi->fb_addr);
        fb_pitch = bi->fb_pitch; fb_w = bi->fb_width; fb_h = bi->fb_height;
        cols = (int)(fb_w / FW); rows = (int)(fb_h / FH);
        mode = MODE_FB;
    } else {
        cols = 80; rows = 25;
        mode = MODE_VGA;
    }
    console_clear();
}
