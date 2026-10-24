/* MNG/MEMO - CMOS real time clock */
#include <memo/arch.h>
#include <memo/rtc.h>

static uint8_t cmos(uint8_t r) { outb(0x70, r); return inb(0x71); }
static uint8_t bcd(uint8_t v) { return (uint8_t)((v & 0x0F) + (v >> 4) * 10); }

void rtc_read(struct rtc_time *t) {
    uint8_t s, m, h, d, mo, y, b;
    while (cmos(0x0A) & 0x80) { }                    /* update in progress */
    s = cmos(0); m = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9); b = cmos(0x0B);
    bool pm = h & 0x80; h &= 0x7F;
    if (!(b & 4)) { s = bcd(s); m = bcd(m); h = bcd(h); d = bcd(d); mo = bcd(mo); y = bcd(y); }
    if (!(b & 2) && pm) h = (uint8_t)((h % 12) + 12);
    t->sec = s; t->min = m; t->hour = h; t->day = d; t->month = mo; t->year = 2000 + y;
}

static int rtc_init(void) { driver_register("rtc", "CMOS real time clock", "ok"); return 0; }
device_initcall(rtc_init);
