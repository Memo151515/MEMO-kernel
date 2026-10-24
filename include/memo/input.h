#pragma once
#include <memo/kernel.h>
#define KEY_UP     0x101
#define KEY_DOWN   0x102
#define KEY_LEFT   0x103
#define KEY_RIGHT  0x104
#define KEY_HOME   0x105
#define KEY_END    0x106
#define KEY_DEL    0x107
#define KEY_PGUP   0x108
#define KEY_PGDN   0x109
void kbd_push(uint16_t key);
int  kbd_poll(void);          /* -1 if empty */
int  kbd_getkey(void);        /* blocks (hlt) until a key is available */

/* Turkish letters that CP437 lacks live in these console glyph slots (patched in at boot) */
#define TR_GBREVE_L 0xF0   /* ğ */
#define TR_GBREVE_U 0xF1   /* Ğ */
#define TR_DOTLESS_I 0xF2  /* ı */
#define TR_DOTTED_I 0xE3   /* İ */
#define TR_SCEDIL_L 0xE4   /* ş */
#define TR_SCEDIL_U 0xE5   /* Ş */
int         kbd_set_layout(const char *name);   /* see kbd_layout_info(); 0 = ok */
const char *kbd_layout(void);
int         kbd_layout_count(void);
void        kbd_layout_info(int i, const char **name, const char **desc);
