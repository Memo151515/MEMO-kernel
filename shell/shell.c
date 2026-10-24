/* MNG/MEMO - interactive shell: line editor, history, tokenizer, '>' / '>>' redirection */
#include <memo/console.h>
#include <memo/input.h>
#include <memo/sched.h>
#include <memo/arch.h>
#include "shell.h"

#define LINE_MAX 256
#define HIST_MAX 32
#define MAX_ARGS 16

static fs_node_t *cwd;
static char history[HIST_MAX][LINE_MAX];
static int hist_count;

/* ---- output with optional redirection ---- */
static char  *redir_buf;
static size_t redir_len, redir_cap;

void sh_write(const char *s, size_t n) {
    if (redir_cap) {
        if (redir_len + n + 1 > redir_cap) {
            size_t nc = ALIGN_UP(redir_len + n + 1, 256) * 2;
            char *nb = krealloc(redir_buf, nc);
            if (!nb) return;
            redir_buf = nb; redir_cap = nc;
        }
        memcpy(redir_buf + redir_len, s, n); redir_len += n;
    } else console_write(s, n);
}
void sh_puts(const char *s) { sh_write(s, strlen(s)); }
int sh_printf(const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sh_write(buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    return n;
}
fs_node_t *sh_cwd(void) { return cwd; }
void sh_set_cwd(fs_node_t *n) { cwd = n; }

void sh_print_history(void) {
    for (int i = 0; i < hist_count; i++) sh_printf("%4d  %s\n", i + 1, history[i]);
}

/* ---- tokenizer: whitespace separated, "double quotes" group words ---- */
static int tokenize(char *line, char **argv) {
    int argc = 0;
    char *p = line;
    while (*p && argc < MAX_ARGS - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"') p++;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        if (*p) *p++ = 0;
    }
    argv[argc] = NULL;
    return argc;
}

static const struct shell_cmd *find_cmd(const char *name) {
    for (const struct shell_cmd *c = __shellcmd_start; c < __shellcmd_end; c++)
        if (!strcmp(c->name, name)) return c;
    return NULL;
}

static void run_line(char *line) {
    char *argv[MAX_ARGS];
    int argc = tokenize(line, argv);
    if (!argc) return;

    /* find redirection operator */
    const char *target = NULL; bool append = false;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], ">") || !strcmp(argv[i], ">>")) {
            append = argv[i][1] == '>';
            if (i + 1 >= argc) { sh_printf("syntax error: missing file after redirection\n"); return; }
            target = argv[i + 1];
            argc = i; argv[i] = NULL;
            break;
        }
    }
    if (!argc) return;

    const struct shell_cmd *c = find_cmd(argv[0]);
    if (!c) { sh_printf("%s: command not found (try 'help')\n", argv[0]); return; }

    if (target) { redir_cap = 256; redir_len = 0; redir_buf = kmalloc(redir_cap); if (!redir_buf) redir_cap = 0; }
    c->fn(argc, argv);
    if (target && redir_cap) {
        size_t len = redir_len; char *buf = redir_buf;
        redir_cap = 0; redir_buf = NULL; redir_len = 0;
        fs_node_t *p; char name[FS_NAME_MAX];
        int rc = fs_split(cwd, target, &p, name);
        if (rc == 0) {
            fs_node_t *f = fs_find_child(p, name);
            if (!f) f = fs_create(p, name, FS_FILE);
            rc = f ? fs_write(f, buf, len, append) : -ENOMEM;
        }
        if (rc) sh_printf("%s: %s\n", target, strerror_k(rc));
        kfree(buf);
    }
}

/* ---- line editor ---- */
static void prompt(void) {
    char path[128];
    fs_path(cwd, path, sizeof path);
    console_set_color(C_LGREEN, C_BLACK); console_puts("root@" MEMO_NODENAME);
    console_set_color(C_LGRAY, C_BLACK);  console_puts(":");
    console_set_color(C_LCYAN, C_BLACK);  console_puts(path);
    console_set_color(C_LGRAY, C_BLACK);  console_puts("# ");
}

static char line[LINE_MAX];
static int  len, pos;
static int  px, py; static uint32_t sc0;       /* prompt cell + scroll counter at prompt time */

static void redraw(int old_len) {
    int cols = console_cols(), rows = console_rows();
    int dy = (int)(console_scroll_count() - sc0);
    int y0 = py - dy; if (y0 < 0) y0 = 0;
    console_set_cursor(px, y0);
    console_write(line, (size_t)len);
    for (int i = len; i < old_len; i++) console_putc(' ');
    dy = (int)(console_scroll_count() - sc0);
    int y1 = py - dy; if (y1 < 0) y1 = 0;
    int lin = px + pos;
    int y = y1 + lin / cols; if (y >= rows) y = rows - 1;
    console_set_cursor(lin % cols, y);
}

static void set_line(const char *s) {
    int old = len;
    len = (int)strlcpy(line, s, LINE_MAX);
    if (len >= LINE_MAX) len = LINE_MAX - 1;
    pos = len;
    redraw(old);
}

static void read_line(void) {
    len = pos = 0; line[0] = 0;
    prompt();
    console_get_cursor(&px, &py); sc0 = console_scroll_count();
    int hidx = hist_count;
    char saved[LINE_MAX]; saved[0] = 0;
    for (;;) {
        int k = kbd_getkey();
        switch (k) {
        case '\n': console_putc('\n'); line[len] = 0; return;
        case 3:    console_puts("^C\n"); len = 0; line[0] = 0; return;      /* Ctrl+C */
        case 12:   console_clear(); prompt(); console_get_cursor(&px, &py); sc0 = console_scroll_count();
                   redraw(len); break;                                       /* Ctrl+L */
        case 21: { int o = len; len = pos = 0; line[0] = 0; redraw(o); break; }   /* Ctrl+U */
        case '\b': if (pos > 0) { int o = len; memmove(line + pos - 1, line + pos, (size_t)(len - pos)); len--; pos--; line[len] = 0;
                         if (pos == len) { console_puts("\b \b"); } else redraw(o); } break;
        case KEY_DEL: if (pos < len) { int o = len; memmove(line + pos, line + pos + 1, (size_t)(len - pos - 1)); len--; line[len] = 0; redraw(o); } break;
        case KEY_LEFT:  if (pos > 0) { pos--; redraw(len); } break;
        case KEY_RIGHT: if (pos < len) { pos++; redraw(len); } break;
        case KEY_HOME:  pos = 0; redraw(len); break;
        case KEY_END:   pos = len; redraw(len); break;
        case KEY_UP:
            if (hidx > 0) { if (hidx == hist_count) strlcpy(saved, line, LINE_MAX); hidx--; set_line(history[hidx]); }
            break;
        case KEY_DOWN:
            if (hidx < hist_count) { hidx++; set_line(hidx == hist_count ? saved : history[hidx]); }
            break;
        case '\t': break;
        default:
            if (((k >= 32 && k < 127) || (k >= 128 && k < 256)) && len < LINE_MAX - 1) {
                if (pos == len) { line[len++] = (char)k; pos++; line[len] = 0; console_putc((char)k); }
                else { int o = len; memmove(line + pos + 1, line + pos, (size_t)(len - pos)); line[pos++] = (char)k; len++; line[len] = 0; redraw(o); }
            }
        }
    }
}

static void add_history(const char *s) {
    if (!*s) return;
    if (hist_count && !strcmp(history[hist_count - 1], s)) return;
    if (hist_count == HIST_MAX) { memmove(history[0], history[1], (HIST_MAX - 1) * LINE_MAX); hist_count--; }
    strlcpy(history[hist_count++], s, LINE_MAX);
}

void shell_task(void) {
    cwd = fs_root();
    fs_node_t *motd = fs_lookup(NULL, "/etc/motd");
    if (motd && motd->data) { console_set_color(C_YELLOW, C_BLACK); console_puts((const char *)motd->data); console_set_color(C_LGRAY, C_BLACK); }
    for (;;) {
        read_line();
        char copy[LINE_MAX];
        strlcpy(copy, line, sizeof copy);
        add_history(copy);
        run_line(copy);
    }
}
