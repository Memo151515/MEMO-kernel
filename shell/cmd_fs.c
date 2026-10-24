/* MNG/MEMO shell - file system commands */
#include <memo/console.h>
#include <memo/arch.h>
#include "shell.h"

static int fail(const char *cmd, const char *arg, int err) {
    sh_printf("%s: %s: %s\n", cmd, arg, strerror_k(err)); return 1;
}

static int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    char p[128]; fs_path(sh_cwd(), p, sizeof p); sh_printf("%s\n", p); return 0;
}
SHELL_CMD(pwd, "", "print working directory", cmd_pwd);

static int cmd_cd(int argc, char **argv) {
    const char *t = argc > 1 ? argv[1] : "/";
    fs_node_t *n = fs_lookup(sh_cwd(), t);
    if (!n) return fail("cd", t, -ENOENT);
    if (n->type != FS_DIR) return fail("cd", t, -ENOTDIR);
    sh_set_cwd(n); return 0;
}
SHELL_CMD(cd, "[dir]", "change directory", cmd_cd);

static int cmd_ls(int argc, char **argv) {
    bool lng = false; const char *path = NULL;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "-l")) lng = true; else path = argv[i]; }
    fs_node_t *d = path ? fs_lookup(sh_cwd(), path) : sh_cwd();
    if (!d) return fail("ls", path, -ENOENT);
    if (d->type == FS_FILE) { sh_printf("%s\n", d->name); return 0; }
    int cnt = 0;
    fs_populate(d);
    for (fs_node_t *c = d->children; c; c = c->sibling) {
        if (lng) sh_printf("%c %8u  %s%s\n", c->type == FS_DIR ? 'd' : '-', (unsigned)c->size, c->name, c->type == FS_DIR ? "/" : "");
        else sh_printf("%s%s  ", c->name, c->type == FS_DIR ? "/" : "");
        cnt++;
    }
    if (!lng && cnt) sh_puts("\n");
    return 0;
}
SHELL_CMD(ls, "[-l] [path]", "list directory contents", cmd_ls);

static int cmd_cat(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: cat <file>...\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        fs_node_t *f = fs_lookup(sh_cwd(), argv[i]);
        if (!f) return fail("cat", argv[i], -ENOENT);
        if (f->type != FS_FILE) return fail("cat", argv[i], -EISDIR);
        fs_load(f);
        if (f->size) { sh_write((const char *)f->data, f->size); if (f->data[f->size - 1] != '\n') sh_puts("\n"); }
    }
    return 0;
}
SHELL_CMD(cat, "<file>...", "print files", cmd_cat);

static int cmd_write(int argc, char **argv) {
    if (argc < 3) { sh_puts("usage: write <file> <text...>\n"); return 1; }
    fs_node_t *p; char name[FS_NAME_MAX];
    int rc = fs_split(sh_cwd(), argv[1], &p, name);
    if (rc) return fail("write", argv[1], rc);
    fs_node_t *f = fs_find_child(p, name);
    if (!f) f = fs_create(p, name, FS_FILE);
    if (!f) return fail("write", argv[1], -ENOMEM);
    char buf[256]; buf[0] = 0;
    for (int i = 2; i < argc; i++) { strlcat(buf, argv[i], sizeof buf); if (i + 1 < argc) strlcat(buf, " ", sizeof buf); }
    strlcat(buf, "\n", sizeof buf);
    rc = fs_write(f, buf, strlen(buf), false);
    return rc ? fail("write", argv[1], rc) : 0;
}
SHELL_CMD(write, "<file> <text...>", "write text to a file (overwrites)", cmd_write);

static int cmd_touch(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: touch <file>...\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        fs_node_t *p; char name[FS_NAME_MAX];
        int rc = fs_split(sh_cwd(), argv[i], &p, name);
        if (rc) return fail("touch", argv[i], rc);
        if (!fs_find_child(p, name) && !fs_create(p, name, FS_FILE)) return fail("touch", argv[i], -ENOMEM);
    }
    return 0;
}
SHELL_CMD(touch, "<file>...", "create empty files", cmd_touch);

static int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: mkdir <dir>...\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        fs_node_t *p; char name[FS_NAME_MAX];
        int rc = fs_split(sh_cwd(), argv[i], &p, name);
        if (rc) return fail("mkdir", argv[i], rc);
        if (fs_find_child(p, name)) return fail("mkdir", argv[i], -EEXIST);
        if (!fs_create(p, name, FS_DIR)) return fail("mkdir", argv[i], -ENOMEM);
    }
    return 0;
}
SHELL_CMD(mkdir, "<dir>...", "create directories", cmd_mkdir);

static int cmd_rmdir(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: rmdir <dir>...\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        fs_node_t *n = fs_lookup(sh_cwd(), argv[i]);
        if (!n) return fail("rmdir", argv[i], -ENOENT);
        if (n->type != FS_DIR) return fail("rmdir", argv[i], -ENOTDIR);
        if (n == sh_cwd()) return fail("rmdir", argv[i], -EINVAL);
        int rc = fs_remove(n);
        if (rc) return fail("rmdir", argv[i], rc);
    }
    return 0;
}
SHELL_CMD(rmdir, "<dir>...", "remove empty directories", cmd_rmdir);

static bool is_ancestor(fs_node_t *a, fs_node_t *n) { for (; n; n = n->parent) if (n == a) return true; return false; }

static int cmd_rm(int argc, char **argv) {
    bool rec = false; int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "-rf")) { rec = true; continue; }
        fs_node_t *n = fs_lookup(sh_cwd(), argv[i]);
        if (!n) { rc = fail("rm", argv[i], -ENOENT); continue; }
        if (is_ancestor(n, sh_cwd())) { rc = fail("rm", argv[i], -EINVAL); continue; }
        if (n->type == FS_DIR && !rec) { rc = fail("rm", argv[i], -EISDIR); continue; }
        int e = rec ? fs_remove_tree(n) : fs_remove(n);
        if (e) rc = fail("rm", argv[i], e);
    }
    if (argc < 2) { sh_puts("usage: rm [-r] <path>...\n"); return 1; }
    return rc;
}
SHELL_CMD(rm, "[-r] <path>...", "remove files (or trees with -r)", cmd_rm);

static int cmd_cp(int argc, char **argv) {
    if (argc != 3) { sh_puts("usage: cp <src> <dst>\n"); return 1; }
    fs_node_t *s = fs_lookup(sh_cwd(), argv[1]);
    if (!s) return fail("cp", argv[1], -ENOENT);
    if (s->type != FS_FILE) return fail("cp", argv[1], -EISDIR);
    fs_load(s);
    fs_node_t *d = fs_lookup(sh_cwd(), argv[2]);
    if (d && d->type == FS_DIR) {
        fs_node_t *e = fs_find_child(d, s->name);
        d = e ? e : fs_create(d, s->name, FS_FILE);
    } else if (!d) {
        fs_node_t *p; char name[FS_NAME_MAX];
        int rc = fs_split(sh_cwd(), argv[2], &p, name);
        if (rc) return fail("cp", argv[2], rc);
        d = fs_create(p, name, FS_FILE);
    }
    if (!d) return fail("cp", argv[2], -ENOMEM);
    if (d == s) return fail("cp", argv[2], -EINVAL);
    int rc = fs_write(d, s->data, s->size, false);
    return rc ? fail("cp", argv[2], rc) : 0;
}
SHELL_CMD(cp, "<src> <dst>", "copy a file", cmd_cp);

static int cmd_mv(int argc, char **argv) {
    if (argc != 3) { sh_puts("usage: mv <src> <dst>\n"); return 1; }
    fs_node_t *s = fs_lookup(sh_cwd(), argv[1]);
    if (!s) return fail("mv", argv[1], -ENOENT);
    if (is_ancestor(s, sh_cwd())) return fail("mv", argv[1], -EINVAL);
    fs_node_t *d = fs_lookup(sh_cwd(), argv[2]);
    int rc;
    if (d && d->type == FS_DIR) rc = fs_move(s, d, s->name);
    else if (d) rc = -EEXIST;
    else {
        fs_node_t *p; char name[FS_NAME_MAX];
        rc = fs_split(sh_cwd(), argv[2], &p, name);
        if (!rc) rc = fs_move(s, p, name);
    }
    return rc ? fail("mv", argv[2], rc) : 0;
}
SHELL_CMD(mv, "<src> <dst>", "move or rename", cmd_mv);

static int cmd_stat(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: stat <path>\n"); return 1; }
    fs_node_t *n = fs_lookup(sh_cwd(), argv[1]);
    if (!n) return fail("stat", argv[1], -ENOENT);
    char p[128]; fs_path(n, p, sizeof p);
    sh_printf("  path: %s\n  type: %s\n  size: %u bytes\n  created: %u ms after boot\n", p,
              n->type == FS_DIR ? "directory" : "file", (unsigned)n->size, n->ctime);
    return 0;
}
SHELL_CMD(stat, "<path>", "show file information", cmd_stat);

static int cmd_wc(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: wc <file>\n"); return 1; }
    fs_node_t *f = fs_lookup(sh_cwd(), argv[1]);
    if (!f) return fail("wc", argv[1], -ENOENT);
    if (f->type != FS_FILE) return fail("wc", argv[1], -EISDIR);
        fs_load(f);
    unsigned lines = 0, words = 0; bool in = false;
    for (size_t i = 0; i < f->size; i++) {
        char c = (char)f->data[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\n' || c == '\t') in = false; else if (!in) { in = true; words++; }
    }
    sh_printf("%u %u %u %s\n", lines, words, (unsigned)f->size, argv[1]);
    return 0;
}
SHELL_CMD(wc, "<file>", "count lines, words and bytes", cmd_wc);

static int cmd_head(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: head <file> [lines]\n"); return 1; }
    fs_node_t *f = fs_lookup(sh_cwd(), argv[1]);
    if (!f) return fail("head", argv[1], -ENOENT);
    if (f->type != FS_FILE) return fail("head", argv[1], -EISDIR);
        fs_load(f);
    long n = 10; if (argc > 2) { bool ok; n = kstrtol(argv[2], 10, &ok); if (!ok) n = 10; }
    size_t i = 0;
    while (i < f->size && n > 0) { sh_write((const char *)&f->data[i], 1); if (f->data[i] == '\n') n--; i++; }
    return 0;
}
SHELL_CMD(head, "<file> [lines]", "print the first lines of a file", cmd_head);

static int cmd_hexdump(int argc, char **argv) {
    if (argc < 2) { sh_puts("usage: hexdump <file>\n"); return 1; }
    fs_node_t *f = fs_lookup(sh_cwd(), argv[1]);
    if (!f) return fail("hexdump", argv[1], -ENOENT);
    if (f->type != FS_FILE) return fail("hexdump", argv[1], -EISDIR);
        fs_load(f);
    for (size_t o = 0; o < f->size; o += 16) {
        sh_printf("%08x  ", (unsigned)o);
        for (size_t i = 0; i < 16; i++) { if (o + i < f->size) sh_printf("%02x ", f->data[o + i]); else sh_puts("   "); }
        sh_puts(" |");
        for (size_t i = 0; i < 16 && o + i < f->size; i++) { char c = (char)f->data[o + i]; sh_printf("%c", (c >= 32 && c < 127) ? c : '.'); }
        sh_puts("|\n");
    }
    return 0;
}
SHELL_CMD(hexdump, "<file>", "hex dump of a file", cmd_hexdump);

static void tree_rec(fs_node_t *d, int depth) {
    fs_populate(d);
    for (fs_node_t *c = d->children; c; c = c->sibling) {
        for (int i = 0; i < depth; i++) sh_puts("  ");
        sh_printf("%s%s\n", c->name, c->type == FS_DIR ? "/" : "");
        if (c->type == FS_DIR && depth < 8) tree_rec(c, depth + 1);
    }
}
static int cmd_tree(int argc, char **argv) {
    fs_node_t *d = argc > 1 ? fs_lookup(sh_cwd(), argv[1]) : sh_cwd();
    if (!d) return fail("tree", argv[1], -ENOENT);
    char p[128]; fs_path(d, p, sizeof p); sh_printf("%s\n", p);
    if (d->type == FS_DIR) tree_rec(d, 1);
    return 0;
}
SHELL_CMD(tree, "[dir]", "show the directory tree", cmd_tree);

static int cmd_df(int argc, char **argv) {
    (void)argc; (void)argv;
    size_t nodes, bytes; fs_stats(&nodes, &bytes);
    sh_printf("ramfs  /  nodes=%u  data=%u bytes (in RAM, volatile)\n", (unsigned)nodes, (unsigned)bytes);
    return 0;
}
SHELL_CMD(df, "", "file system usage", cmd_df);

static int cmd_mount(int argc, char **argv) {
    if (argc == 1) {
        sh_puts("ramfs on /\n");
        return 0;
    }
    if (argc != 3) { sh_puts("usage: mount <disk 0-3> <dir>   (mounts an ext2 disk read-only)\n"); return 1; }
    bool ok; long d = kstrtol(argv[1], 10, &ok);
    fs_node_t *dir = fs_lookup(sh_cwd(), argv[2]);
    if (!ok || !dir) return fail("mount", argv[2], -ENOENT);
    int rc = ext2_mount((int)d, dir);
    if (rc) return fail("mount", argv[1], rc);
    sh_printf("mounted disk %ld (ext2, read-only) on %s\n", d, argv[2]);
    return 0;
}
SHELL_CMD(mount, "[disk dir]", "mount an ext2 disk, or list mounts", cmd_mount);
