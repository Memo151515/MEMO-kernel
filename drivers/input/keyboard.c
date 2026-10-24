/* MNG/MEMO - PS/2 keyboard (scancode set 1): shift, caps, ctrl, arrows, key queue */
#include <memo/arch.h>
#include <memo/input.h>

#define QSIZE 256
static volatile uint16_t q[QSIZE];
static volatile uint32_t qh, qt;

void kbd_push(uint16_t k) {
    uintptr_t fl = irq_save();
    uint32_t nt = (qt + 1) % QSIZE;
    if (nt != qh) { q[qt] = k; qt = nt; }
    irq_restore(fl);
}
int kbd_poll(void) {
    uintptr_t fl = irq_save();
    int k = -1;
    if (qh != qt) { k = q[qh]; qh = (qh + 1) % QSIZE; }
    irq_restore(fl);
    return k;
}
int kbd_getkey(void) {
    for (;;) {
        int k = kbd_poll();
        if (k >= 0) return k;
        hlt();
    }
}

/* ---- layouts ----------------------------------------------------------------------------
 * Characters are CP437 codes (the console font), written as octal escapes:
 *   \204 ä  \224 ö  \201 ü  \216 Ä  \231 Ö  \232 Ü  \341 ß  \202 é  \212 è  \205 à  \227 ù  \207 ç
 *   \244 ñ  \245 Ñ  \250 ¿  \255 ¡  \234 £  \370 °  \025 §  \375 ²  \215 ì  \225 ò  \346 µ
 * Each layout lists the keys row by row (scancodes 0x02-0x0D, 0x10-0x1B, 0x1E-0x29, 0x2C-0x35,
 * then 0x2B and 0x56) for the normal and the shifted state, plus a short AltGr table.
 * Add a layout by adding one entry to layouts[] below.
 */
#define NKEYS 0x59
#define NONE  '\001'

struct altgr { uint8_t sc; unsigned char c; };
struct layout {
    const char *name, *desc;
    const char *n[5], *s[5];
    const struct altgr *ag;
};

static const struct altgr ag_none[] = { {0, 0} };
static const struct altgr ag_us[]   = { {0, 0} };
static const struct altgr ag_de[] = { {0x08,'{'},{0x09,'['},{0x0A,']'},{0x0B,'}'},{0x0C,'\\'},{0x10,'@'},{0x1B,'~'},{0x56,'|'},{0,0} };
static const struct altgr ag_fr[] = { {0x04,'#'},{0x05,'{'},{0x06,'['},{0x07,'|'},{0x08,'`'},{0x09,'\\'},{0x0A,'^'},{0x0B,'@'},{0x0C,']'},{0x0D,'}'},{0,0} };
static const struct altgr ag_es[] = { {0x02,'|'},{0x03,'@'},{0x04,'#'},{0x1A,'['},{0x1B,']'},{0x28,'{'},{0x29,'\\'},{0x2B,'}'},{0,0} };
static const struct altgr ag_it[] = { {0x1A,'['},{0x1B,']'},{0x27,'@'},{0x28,'#'},{0,0} };
static const struct altgr ag_tr[] = { {0x04,'#'},{0x05,'$'},{0x08,'{'},{0x09,'['},{0x0A,']'},{0x0B,'}'},{0x0C,'\\'},{0x0D,'|'},{0x10,'@'},{0x1B,'~'},{0x56,'|'},{0,0} };

static const struct layout layouts[] = {
 { "us", "US English (QWERTY)",
   { "1234567890-=", "qwertyuiop[]", "asdfghjkl;'`", "zxcvbnm,./", "\\\001" },
   { "!@#$%^&*()_+", "QWERTYUIOP{}", "ASDFGHJKL:\"~", "ZXCVBNM<>?", "|\001" }, ag_us },
 { "uk", "UK English",
   { "1234567890-=", "qwertyuiop[]", "asdfghjkl;'`", "zxcvbnm,./", "#\\" },
   { "!\"\234$%^&*()_+", "QWERTYUIOP{}", "ASDFGHJKL:@\252", "ZXCVBNM<>?", "~|" }, ag_none },
 { "de", "German (QWERTZ)",
   { "1234567890\341'", "qwertzuiop\201+", "asdfghjkl\224\204^", "yxcvbnm,.-", "#<" },
   { "!\"\025$%&/()=?`", "QWERTZUIOP\232*", "ASDFGHJKL\231\216\370", "YXCVBNM;:_", "'>" }, ag_de },
 { "fr", "French (AZERTY)",
   { "&\202\"'(-\212_\207\205)=", "azertyuiop^$", "qsdfghjklm\227\375", "wxcvbn,;:!", "*<" },
   { "1234567890\370+", "AZERTYUIOP\"\234", "QSDFGHJKLM%\375", "WXCVBN?./\025", "\346>" }, ag_fr },
 { "es", "Spanish",
   { "1234567890'\255", "qwertyuiop`+", "asdfghjkl\244'\247", "zxcvbnm,.-", "\207<" },
   { "!\"\372$%&/()=?\250", "QWERTYUIOP^*", "ASDFGHJKL\245\"\246", "ZXCVBNM;:_", "\200>" }, ag_es },
 { "it", "Italian",
   { "1234567890'\215", "qwertyuiop\212+", "asdfghjkl\225\205\\", "zxcvbnm,.-", "\227<" },
   { "!\"\234$%&/()=?^", "QWERTYUIOP\202*", "ASDFGHJKL\207\370|", "ZXCVBNM;:_", "\025>" }, ag_it },
 { "tr", "Turkish Q",
   { "1234567890*-", "qwertyu\362op\360\201", "asdfghjkl\364i\"", "zxcvbnm\224\207.", ",<" },
   { "!'^+%&/()=?_", "QWERTYUIOP\361\232", "ASDFGHJKL\365\363\"", "ZXCVBNM\231\200:", ";>" }, ag_tr },
 { "dvorak", "US Dvorak",
   { "1234567890[]", "',.pyfgcrl/=", "aoeuidhtns-`", ";qjkxbmwvz", "\\\001" },
   { "!@#$%^&*(){}", "\"<>PYFGCRL?+", "AOEUIDHTNS_~", ":QJKXBMWVZ", "|\001" }, ag_none },
 { "colemak", "US Colemak",
   { "1234567890-=", "qwfpgjluy;[]", "arstdhneio'`", "zxcvbkm,./", "\\\001" },
   { "!@#$%^&*()_+", "QWFPGJLUY:{}", "ARSTDHNEIO\"~", "ZXCVBKM<>?", "|\001" }, ag_none },
};
#define NLAYOUTS ((int)ARRAY_SIZE(layouts))

static char cur_n[NKEYS], cur_s[NKEYS], cur_a[NKEYS];
static int  cur_layout;

static void apply_layout(const struct layout *L) {
    static const uint8_t start[4] = { 0x02, 0x10, 0x1E, 0x2C };
    static const uint8_t count[4] = { 12, 12, 12, 10 };
    memset(cur_n, 0, NKEYS); memset(cur_s, 0, NKEYS); memset(cur_a, 0, NKEYS);
    cur_n[0x01] = cur_s[0x01] = 27;  cur_n[0x0E] = cur_s[0x0E] = '\b';
    cur_n[0x0F] = cur_s[0x0F] = '\t'; cur_n[0x1C] = cur_s[0x1C] = '\n';
    cur_n[0x39] = cur_s[0x39] = ' ';  cur_n[0x37] = cur_s[0x37] = '*';
    for (int r = 0; r < 4; r++)
        for (int i = 0; i < count[r]; i++) {
            char n = L->n[r][i], s = L->s[r][i];
            cur_n[start[r] + i] = n == NONE ? 0 : n;
            cur_s[start[r] + i] = s == NONE ? 0 : s;
        }
    cur_n[0x2B] = L->n[4][0] == NONE ? 0 : L->n[4][0];  cur_s[0x2B] = L->s[4][0] == NONE ? 0 : L->s[4][0];
    cur_n[0x56] = L->n[4][1] == NONE ? 0 : L->n[4][1];  cur_s[0x56] = L->s[4][1] == NONE ? 0 : L->s[4][1];
    for (const struct altgr *a = L->ag; a->sc; a++) cur_a[a->sc] = (char)a->c;
}

int kbd_set_layout(const char *name) {
    if (!strcmp(name, "trq")) name = "tr";
    for (int i = 0; i < NLAYOUTS; i++)
        if (!strcmp(layouts[i].name, name)) { cur_layout = i; apply_layout(&layouts[i]); return 0; }
    return -1;
}
const char *kbd_layout(void) { return layouts[cur_layout].name; }
int  kbd_layout_count(void) { return NLAYOUTS; }
void kbd_layout_info(int i, const char **name, const char **desc) { *name = layouts[i].name; *desc = layouts[i].desc; }

static bool is_letter(unsigned char c) {
    if (c >= 'a' && c <= 'z') return true;
    switch (c) {
    case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87: case 0x88: case 0x89: case 0x8A:
    case 0x8B: case 0x8C: case 0x94: case 0x95: case 0x96: case 0x97: case 0xA4:
    case TR_GBREVE_L: case TR_DOTLESS_I: case TR_SCEDIL_L: return true;
    }
    return false;
}

static bool shift, ctrl, caps, altgr, ext;

static void kbd_irq(struct regs *r) {
    (void)r;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) { ext = true; return; }
    bool release = sc & 0x80;
    uint8_t code = sc & 0x7F;
    bool was_ext = ext; ext = false;

    if (!was_ext) {
        if (code == 0x2A || code == 0x36) { shift = !release; return; }
        if (code == 0x1D) { ctrl = !release; return; }
        if (code == 0x3A) { if (!release) caps = !caps; return; }
    } else {
        if (code == 0x1D) { ctrl = !release; return; }
        if (code == 0x38) { altgr = !release; return; }          /* right Alt = AltGr */
    }
    if (release) return;

    if (was_ext) {
        switch (code) {
        case 0x48: kbd_push(KEY_UP); break;    case 0x50: kbd_push(KEY_DOWN); break;
        case 0x4B: kbd_push(KEY_LEFT); break;  case 0x4D: kbd_push(KEY_RIGHT); break;
        case 0x47: kbd_push(KEY_HOME); break;  case 0x4F: kbd_push(KEY_END); break;
        case 0x53: kbd_push(KEY_DEL); break;   case 0x49: kbd_push(KEY_PGUP); break;
        case 0x51: kbd_push(KEY_PGDN); break;
        case 0x1C: kbd_push('\n'); break;      /* keypad Enter */
        case 0x35: kbd_push('/'); break;       /* keypad / */
        }
        return;
    }
    if (code >= NKEYS) return;
    unsigned char c = 0;
    if (altgr && cur_a[code]) c = (unsigned char)cur_a[code];
    else {
        unsigned char base = (unsigned char)cur_n[code];
        bool up = shift;
        if (is_letter(base)) up = shift ^ caps;
        c = (unsigned char)(up ? cur_s[code] : cur_n[code]);
    }
    if (!c) return;
    if (ctrl && c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 1);
    else if (ctrl && c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 1);
    kbd_push(c);
}

static int kbd_init(void) {
    if (kbd_set_layout(DEFAULT_KEYMAP)) kbd_set_layout("us");
    irq_install_handler(1, kbd_irq);
    driver_register("i8042-kbd", "PS/2 keyboard, layouts via loadkeys", "ok");
    return 0;
}
device_initcall(kbd_init);
