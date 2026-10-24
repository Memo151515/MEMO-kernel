/* MNG/MEMO - ext2 / ext3 / ext4 file system, read-only.
 * Supports classic block maps and ext4 extent trees, 64-bit group descriptors, flex_bg, htree
 * directories (read linearly) and any metadata checksum (not verified). Journal is ignored. */
#include <memo/fs.h>
#include <memo/block.h>

struct ext2_mnt {
    int disk; uint32_t bsize, inodes_per_group, inode_size, first_data_block, gdt_block, ngroups, inodes_count;
    uint32_t desc_size, incompat;
};
static struct ext2_mnt mnts[4];
static int nmnt;

static uint32_t le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int read_block(struct ext2_mnt *m, uint64_t blk, void *buf) {
    uint32_t spb = m->bsize / 512;
    uint64_t lba = blk * spb;
    if (lba > 0xFFFFFFF0ull) return -EIO;                       /* ATA driver is LBA28/32 for now */
    return ata_read(m->disk, (uint32_t)lba, spb, buf);
}

#define EXT4_EXTENTS_FL 0x80000u
struct inode { uint16_t mode; uint32_t size, flags; uint32_t block[15]; uint8_t raw[60]; };

static int read_inode(struct ext2_mnt *m, uint32_t ino, struct inode *out) {
    if (ino == 0 || ino > m->inodes_count) return -EINVAL;
    uint32_t grp = (ino - 1) / m->inodes_per_group, idx = (ino - 1) % m->inodes_per_group;
    uint8_t *buf = kmalloc(m->bsize);
    if (!buf) return -ENOMEM;
    int rc = read_block(m, m->gdt_block + (grp * m->desc_size) / m->bsize, buf);
    if (rc) { kfree(buf); return rc; }
    const uint8_t *gd = buf + (grp * m->desc_size) % m->bsize;
    uint64_t table = le32(gd + 8);
    if (m->desc_size >= 64) table |= (uint64_t)le32(gd + 0x28) << 32;
    uint32_t off = idx * m->inode_size;
    rc = read_block(m, table + off / m->bsize, buf);
    if (rc) { kfree(buf); return rc; }
    const uint8_t *p = buf + off % m->bsize;
    out->mode = le16(p); out->size = le32(p + 4); out->flags = le32(p + 32);
    if ((out->mode & 0xF000) == 0x8000 && le32(p + 108)) out->size = 0xFFFFFFF0u;   /* > 4 GiB: not supported */
    for (int i = 0; i < 15; i++) out->block[i] = le32(p + 40 + i * 4);
    memcpy(out->raw, p + 40, 60);
    kfree(buf);
    return 0;
}

/* ext4: walk the extent tree stored in the inode (i_block) */
static int extent_map(struct ext2_mnt *m, const struct inode *in, uint32_t lblk, uint64_t *out) {
    const uint8_t *node = in->raw;
    uint8_t *buf = NULL;
    *out = 0;
    for (int guard = 0; guard < 8; guard++) {
        if (le16(node) != 0xF30A) { kfree(buf); return -EIO; }
        uint16_t entries = le16(node + 2), depth = le16(node + 6);
        if (depth == 0) {
            for (uint16_t i = 0; i < entries; i++) {
                const uint8_t *e = node + 12 + 12 * i;
                uint32_t start = le32(e); uint32_t len = le16(e + 4);
                bool uninit = len > 32768; if (uninit) len -= 32768;
                if (lblk >= start && lblk - start < len) {
                    if (!uninit) *out = (((uint64_t)le16(e + 6) << 32) | le32(e + 8)) + (lblk - start);
                    kfree(buf); return 0;
                }
            }
            kfree(buf); return 0;                                  /* hole */
        }
        const uint8_t *sel = NULL;
        for (uint16_t i = 0; i < entries; i++) { const uint8_t *e = node + 12 + 12 * i; if (le32(e) <= lblk) sel = e; }
        if (!sel) { kfree(buf); return 0; }
        uint64_t child = le32(sel + 4) | ((uint64_t)le16(sel + 8) << 32);
        if (!buf) { buf = kmalloc(m->bsize); if (!buf) return -ENOMEM; }
        if (read_block(m, child, buf)) { kfree(buf); return -EIO; }
        node = buf;
    }
    kfree(buf);
    return -EIO;
}

/* logical block n of an inode -> physical block (0 = hole) */
static int bmap(struct ext2_mnt *m, const struct inode *in, uint32_t n, uint64_t *out) {
    uint32_t ppb = m->bsize / 4;
    uint8_t *buf = NULL;
    *out = 0;
    if (in->flags & EXT4_EXTENTS_FL) return extent_map(m, in, n, out);
    if (n < 12) { *out = in->block[n]; return 0; }
    n -= 12;
    uint32_t blk, levels;
    if (n < ppb) { blk = in->block[12]; levels = 1; }
    else if ((n -= ppb) < ppb * ppb) { blk = in->block[13]; levels = 2; }
    else { n -= ppb * ppb; blk = in->block[14]; levels = 3; }
    buf = kmalloc(m->bsize);
    if (!buf) return -ENOMEM;
    for (uint32_t l = levels; l > 0 && blk; l--) {
        if (read_block(m, blk, buf)) { kfree(buf); return -EIO; }
        uint32_t div = 1; for (uint32_t i = 1; i < l; i++) div *= ppb;
        blk = le32(buf + ((n / div) % ppb) * 4);
    }
    kfree(buf);
    *out = blk;
    return 0;
}

static int read_data(struct ext2_mnt *m, const struct inode *in, uint8_t *dst, size_t size) {
    uint8_t *buf = kmalloc(m->bsize);
    if (!buf) return -ENOMEM;
    for (size_t off = 0; off < size; off += m->bsize) {
        uint64_t blk; int rc = bmap(m, in, (uint32_t)(off / m->bsize), &blk);
        size_t c = MIN((size_t)m->bsize, size - off);
        if (rc) { kfree(buf); return rc; }
        if (!blk) { memset(dst + off, 0, c); continue; }
        if (read_block(m, blk, buf)) { kfree(buf); return -EIO; }
        memcpy(dst + off, buf, c);
    }
    kfree(buf);
    return 0;
}

static int ext2_load_file(fs_node_t *f) {
    struct ext2_mnt *m = &mnts[f->mount];
    struct inode in;
    int rc = read_inode(m, f->ino, &in);
    if (rc) return rc;
    f->data = kmalloc(in.size + 1);
    if (!f->data) return -ENOMEM;
    rc = read_data(m, &in, f->data, in.size);
    if (rc) { kfree(f->data); f->data = NULL; return rc; }
    f->data[in.size] = 0; f->size = in.size; f->cap = in.size + 1;
    return 0;
}

static int ext2_load_dir(fs_node_t *dir) {
    struct ext2_mnt *m = &mnts[dir->mount];
    struct inode din;
    int rc = read_inode(m, dir->ino, &din);
    if (rc) return rc;
    uint8_t *d = kmalloc(din.size + 1);
    if (!d) return -ENOMEM;
    rc = read_data(m, &din, d, din.size);
    for (uint32_t off = 0; !rc && off + 8 <= din.size; ) {
        uint32_t ino = le32(d + off); uint16_t rec = le16(d + off + 4); uint8_t nl = d[off + 6];
        if (rec < 8) break;
        if (ino && nl && nl < FS_NAME_MAX) {
            char name[FS_NAME_MAX]; memcpy(name, d + off + 8, nl); name[nl] = 0;
            if (strcmp(name, ".") && strcmp(name, "..")) {
                struct inode in;
                if (!read_inode(m, ino, &in)) {
                    int type = (in.mode & 0xF000) == 0x4000 ? FS_DIR : ((in.mode & 0xF000) == 0x8000 ? FS_FILE : 0);
                    if (type) fs_create_be(dir, name, type, ino, in.size);
                }
            }
        }
        off += rec;
    }
    kfree(d);
    return rc;
}

static const struct fs_backend ext2_be = { "ext2/3/4", ext2_load_dir, ext2_load_file };

int ext2_mount(int disk, fs_node_t *dir) {
    if (nmnt >= 4 || dir->type != FS_DIR || dir->children) return -EINVAL;
    uint8_t sb[1024];
    int rc = ata_read(disk, 2, 2, sb);
    if (rc) return rc;
    if (le16(sb + 56) != 0xEF53) return -EINVAL;
    struct ext2_mnt *m = &mnts[nmnt];
    m->disk = disk; m->bsize = 1024u << le32(sb + 24);
    m->inodes_per_group = le32(sb + 40); m->inodes_count = le32(sb + 0);
    m->first_data_block = le32(sb + 20); m->gdt_block = m->first_data_block + 1;
    m->inode_size = le32(sb + 76) >= 1 ? le16(sb + 88) : 128;
    m->incompat = le32(sb + 96);
    m->desc_size = (m->incompat & 0x80) ? le16(sb + 254) : 32;               /* INCOMPAT_64BIT */
    if (m->desc_size < 32) m->desc_size = 32;
    if (m->incompat & (0x1 | 0x8 | 0x10 | 0x8000)) return -EINVAL;           /* compression, journal dev, meta_bg, inline_data */
    if (m->bsize > 65536 || !m->inodes_per_group || !m->inode_size) return -EINVAL;
    dir->be = &ext2_be; dir->mount = (uint32_t)nmnt; dir->ino = 2; dir->loaded = false;
    nmnt++;
    return 0;
}

/* mount the first ext2 disk found (other than the boot disk) on /disk */
void ext2_automount(void) {
    for (int d = 1; d < ata_count(); d++) {
        char model[41]; uint32_t sec; bool present;
        if (!ata_info(d, model, sizeof model, &sec, &present) || !present) continue;
        fs_node_t *dir = fs_find_child(fs_root(), "disk");
        if (!dir) dir = fs_create(fs_root(), "disk", FS_DIR);
        if (dir && ext2_mount(d, dir) == 0) { printk("ext2/ext3/ext4: disk %d mounted on /disk (read-only)\n", d); return; }
    }
}
