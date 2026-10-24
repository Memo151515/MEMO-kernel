/* MNG/MEMO - in-memory file system with real directories */
#include <memo/fs.h>
#include <memo/sched.h>

static fs_node_t *root;
static size_t n_nodes, n_bytes;

static fs_node_t *node_new(const char *name, int type) {
    fs_node_t *n = kcalloc(1, sizeof *n);
    if (!n) return NULL;
    strlcpy(n->name, name, FS_NAME_MAX);
    n->type = (uint8_t)type;
    n->ctime = uptime_ms();
    n_nodes++;
    return n;
}

fs_node_t *fs_root(void) { return root; }

void fs_populate(fs_node_t *dir) {
    if (dir && dir->type == FS_DIR && dir->be && !dir->loaded) { dir->loaded = true; dir->be->load_dir(dir); }
}
int fs_load(fs_node_t *f) {
    if (f->type != FS_FILE || !f->be || f->loaded) return 0;
    f->loaded = true;
    return f->be->load_file(f);
}
fs_node_t *fs_create_be(fs_node_t *parent, const char *name, int type, uint32_t ino, size_t size) {
    fs_node_t *n = node_new(name, type);
    if (!n) return NULL;
    n->parent = parent; n->sibling = parent->children; parent->children = n;
    n->be = parent->be; n->mount = parent->mount; n->ino = ino; n->size = size;
    return n;
}

fs_node_t *fs_find_child(fs_node_t *dir, const char *name) {
    if (!dir || dir->type != FS_DIR) return NULL;
    fs_populate(dir);
    for (fs_node_t *c = dir->children; c; c = c->sibling) if (!strcmp(c->name, name)) return c;
    return NULL;
}

static fs_node_t *walk(fs_node_t *cwd, const char *path, size_t *consumed_all_but_last, char *last) {
    (void)consumed_all_but_last; (void)last;
    fs_node_t *cur = (path[0] == '/') ? root : (cwd ? cwd : root);
    char comp[FS_NAME_MAX];
    while (*path) {
        while (*path == '/') path++;
        if (!*path) break;
        size_t l = 0;
        while (path[l] && path[l] != '/') l++;
        if (l >= FS_NAME_MAX) return NULL;
        memcpy(comp, path, l); comp[l] = 0; path += l;
        if (!strcmp(comp, ".")) continue;
        if (!strcmp(comp, "..")) { if (cur->parent) cur = cur->parent; continue; }
        cur = fs_find_child(cur, comp);
        if (!cur) return NULL;
    }
    return cur;
}

fs_node_t *fs_lookup(fs_node_t *cwd, const char *path) { return walk(cwd, path, NULL, NULL); }

/* split "a/b/c" into the directory node for "a/b" and the leaf name "c" */
int fs_split(fs_node_t *cwd, const char *path, fs_node_t **parent, char *name) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;
    size_t i = len;
    while (i > 0 && path[i - 1] != '/') i--;
    size_t nl = len - i;
    if (nl == 0 || nl >= FS_NAME_MAX) return nl ? -ENAMETOOLONG : -EINVAL;
    memcpy(name, path + i, nl); name[nl] = 0;
    if (!strcmp(name, ".") || !strcmp(name, "..")) return -EINVAL;
    char dir[128];
    size_t dl = i;
    if (dl >= sizeof dir) return -ENAMETOOLONG;
    memcpy(dir, path, dl); dir[dl] = 0;
    fs_node_t *p = dl ? fs_lookup(cwd, dir) : (cwd ? cwd : root);
    if (!p) return -ENOENT;
    if (p->type != FS_DIR) return -ENOTDIR;
    *parent = p;
    return 0;
}

fs_node_t *fs_create(fs_node_t *parent, const char *name, int type) {
    if (!parent || parent->type != FS_DIR || parent->be || fs_find_child(parent, name)) return NULL;
    fs_node_t *n = node_new(name, type);
    if (!n) return NULL;
    n->parent = parent;
    n->sibling = parent->children;
    parent->children = n;
    return n;
}

static void unlink_node(fs_node_t *n) {
    fs_node_t **pp = &n->parent->children;
    while (*pp && *pp != n) pp = &(*pp)->sibling;
    if (*pp) *pp = n->sibling;
}

int fs_remove(fs_node_t *n) {
    if (!n->parent) return -EINVAL;                         /* root */
    if (n->be) return -EROFS;
    if (n->type == FS_DIR && n->children) return -ENOTEMPTY;
    unlink_node(n);
    n_bytes -= n->cap;
    kfree(n->data);
    kfree(n);
    n_nodes--;
    return 0;
}

int fs_remove_tree(fs_node_t *n) {
    fs_populate(n);
    while (n->children) fs_remove_tree(n->children);
    return fs_remove(n);
}

int fs_write(fs_node_t *f, const void *data, size_t len, bool append) {
    if (f->type != FS_FILE) return -EISDIR;
    if (f->be) return -EROFS;
    size_t off = append ? f->size : 0;
    size_t need = off + len;
    if (need > f->cap) {
        size_t ncap = ALIGN_UP(need + 1, 64);
        uint8_t *nd = krealloc(f->data, ncap);
        if (!nd) return -ENOMEM;
        n_bytes += ncap - f->cap;
        f->data = nd; f->cap = ncap;
    }
    if (len) memcpy(f->data + off, data, len);
    f->size = need;
    f->data[need] = 0;
    return 0;
}

int fs_move(fs_node_t *n, fs_node_t *np, const char *new_name) {
    if (!n->parent || np->type != FS_DIR || n->be || np->be) return -EROFS;
    for (fs_node_t *p = np; p; p = p->parent) if (p == n) return -EINVAL;   /* into itself */
    if (fs_find_child(np, new_name)) return -EEXIST;
    unlink_node(n);
    strlcpy(n->name, new_name, FS_NAME_MAX);
    n->parent = np; n->sibling = np->children; np->children = n;
    return 0;
}

void fs_path(fs_node_t *n, char *buf, size_t size) {
    if (!n->parent) { strlcpy(buf, "/", size); return; }
    char tmp[256]; size_t pos = sizeof tmp - 1; tmp[pos] = 0;
    for (fs_node_t *c = n; c->parent; c = c->parent) {
        size_t l = strlen(c->name);
        if (pos < l + 1) break;
        pos -= l; memcpy(tmp + pos, c->name, l);
        tmp[--pos] = '/';
    }
    strlcpy(buf, tmp + pos, size);
}

void fs_stats(size_t *nodes, size_t *bytes) { *nodes = n_nodes; *bytes = n_bytes; }

static void put(const char *path, const char *text) {
    fs_node_t *p; char name[FS_NAME_MAX];
    if (fs_split(root, path, &p, name) == 0) {
        fs_node_t *f = fs_create(p, name, FS_FILE);
        if (f) fs_write(f, text, strlen(text), false);
    }
}

void fs_init(void) {
    root = node_new("", FS_DIR);
    static const char *const dirs[] = { "bin", "dev", "etc", "home", "proc", "tmp", "usr", "var" };
    for (size_t i = 0; i < ARRAY_SIZE(dirs); i++) fs_create(root, dirs[i], FS_DIR);
    put("/etc/hostname", MEMO_NODENAME "\n");
    put("/etc/os-release", "NAME=\"" MEMO_SYSNAME "\"\nVERSION=\"" MEMO_VERSION "\"\nKERNEL=" MEMO_KERNEL_NAME
        "\nUSERLAND=" MEMO_USERLAND_NAME " (" MEMO_USERLAND_LONG ")\n");
    put("/etc/motd", "Welcome to " MEMO_SYSNAME " " MEMO_VERSION " - " MEMO_USERLAND_LONG ".\nType 'help' for the command list.\n");
    put("/home/readme.txt", "This is the MEMO RAM file system. Files here are lost on reboot.\n");
}
