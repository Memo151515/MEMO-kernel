/* MNG/MEMO - vsnprintf (supports %c %s %d %i %u %x %X %p %o %% , flags - 0 + , width, .prec, l ll z) */
#include <memo/kernel.h>

/* 64-bit divide without libgcc (needed on i386) */
static uint32_t do_div(uint64_t *n, uint32_t base) {
    uint64_t v = *n;
    if (!(v >> 32)) { uint32_t x = (uint32_t)v; *n = x / base; return x % base; }
    uint64_t q = 0; uint32_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | (uint32_t)((v >> i) & 1);
        if (r >= base) { r -= base; q |= 1ULL << i; }
    }
    *n = q;
    return r;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    char dummy; char *p = size ? buf : &dummy; char *end = size ? buf + size - 1 : &dummy;
    size_t total = 0;
#define PUT(ch) do { if (p < end) *p++ = (ch); total++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        fmt++;
        bool left = false, zero = false, plus = false, alt = false;
        for (;; fmt++) {
            if (*fmt == '-') left = true; else if (*fmt == '0') zero = true;
            else if (*fmt == '+') plus = true; else if (*fmt == '#') alt = true; else break;
        }
        int width = 0, prec = -1, lng = 0;
        if (*fmt == '*') { width = va_arg(ap, int); if (width < 0) { left = true; width = -width; } fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
        }
        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') { if (*fmt != 'h') lng++; fmt++; }

        char tmp[24]; const char *s = tmp; int len = 0; char sign = 0;
        int base = 10; bool upper = false, isnum = false, neg = false; uint64_t v = 0;
        switch (*fmt) {
        case 'c': tmp[0] = (char)va_arg(ap, int); len = 1; break;
        case 's': s = va_arg(ap, const char *); if (!s) s = "(null)";
                  len = (int)strnlen(s, prec < 0 ? (size_t)-1 : (size_t)prec); break;
        case 'd': case 'i': {
            int64_t sv;
            if (lng >= 2) sv = va_arg(ap, long long);
            else if (lng == 1) sv = va_arg(ap, long);
            else sv = va_arg(ap, int);
            if (sv < 0) { neg = true; v = (uint64_t)(-sv); } else v = (uint64_t)sv;
            isnum = true; break; }
        case 'u': case 'x': case 'X': case 'o': case 'p':
            if (*fmt == 'p') { v = (uint64_t)(uintptr_t)va_arg(ap, void *); base = 16; PUT('0'); PUT('x'); width -= 2; if (width < 0) width = 0; zero = zero; }
            else {
                if (lng >= 2) v = va_arg(ap, unsigned long long);
                else if (lng == 1) v = va_arg(ap, unsigned long);
                else v = va_arg(ap, unsigned int);
                base = *fmt == 'o' ? 8 : (*fmt == 'u' ? 10 : 16);
                upper = *fmt == 'X';
            }
            isnum = true; break;
        case '%': tmp[0] = '%'; len = 1; break;
        default: tmp[0] = '%'; tmp[1] = *fmt ? *fmt : '?'; len = 2; if (!*fmt) fmt--; break;
        }
        if (isnum) {
            char rev[24]; int n = 0; uint64_t v0 = v;
            const char *dg = upper ? "0123456789ABCDEF" : "0123456789abcdef";
            if (v == 0) rev[n++] = '0';
            while (v) rev[n++] = dg[do_div(&v, (uint32_t)base)];
            if (prec >= 0) { while (n < prec && n < 22) rev[n++] = '0'; zero = false; }
            if (neg) sign = '-'; else if (plus) sign = '+';
            len = 0;
            if (sign) tmp[len++] = sign;
            if (alt && base == 16 && v0 != 0) { tmp[len++] = '0'; tmp[len++] = upper ? 'X' : 'x'; }
            int signlen = len;
            while (width > n + signlen && zero && !left) { rev[n++] = '0'; }
            while (n) tmp[len++] = rev[--n];
        }
        if (!left) for (int i = len; i < width; i++) PUT(' ');
        for (int i = 0; i < len; i++) PUT(s[i]);
        if (left) for (int i = len; i < width; i++) PUT(' ');
    }
    if (size) *p = 0;
    return (int)total;
#undef PUT
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
