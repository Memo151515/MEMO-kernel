#pragma once
#include <memo/kernel.h>

#define FS_NAME_MAX 32
enum { FS_FILE = 1, FS_DIR = 2 };

struct fs_node;
struct fs_backend {                          /* read-only disk file systems (ext2) */
    const char *name;
    int (*load_dir)(struct fs_node *dir);
    int (*load_file)(struct fs_node *f);
};

typedef struct fs_node {
    char name[FS_NAME_MAX];
    uint8_t type;
    struct fs_node *parent, *children, *sibling;
    uint8_t *data;
    size_t size, cap;
    uint32_t ctime;                         /* uptime ms at creation */
    const struct fs_backend *be;            /* NULL = plain RAM node */
    uint32_t ino, mount;                    /* backend inode and mount id */
    bool loaded;                            /* backend: directory listed / file data cached */
} fs_node_t;

void       fs_init(void);
fs_node_t *fs_root(void);
fs_node_t *fs_find_child(fs_node_t *dir, const char *name);
fs_node_t *fs_lookup(fs_node_t *cwd, const char *path);
int        fs_split(fs_node_t *cwd, const char *path, fs_node_t **parent, char *name);
fs_node_t *fs_create(fs_node_t *parent, const char *name, int type);
int        fs_remove(fs_node_t *n);                         /* empty dirs / files only */
int        fs_remove_tree(fs_node_t *n);
int        fs_write(fs_node_t *f, const void *data, size_t len, bool append);
int        fs_move(fs_node_t *n, fs_node_t *new_parent, const char *new_name);
void       fs_path(fs_node_t *n, char *buf, size_t size);
fs_node_t *fs_create_be(fs_node_t *parent, const char *name, int type, uint32_t ino, size_t size);
void       fs_populate(fs_node_t *dir);              /* list a backend directory (no-op for RAM) */
int        fs_load(fs_node_t *f);                    /* make f->data valid (no-op for RAM files) */
int        ext2_mount(int disk, fs_node_t *dir);
void       ext2_automount(void);
void       fs_stats(size_t *nodes, size_t *bytes);
