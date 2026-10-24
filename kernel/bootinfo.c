#include <memo/bootinfo.h>

static struct boot_info  bi;
static struct mmap_entry mm[MMAP_MAX];
static uint8_t           font[4096];
static bool              valid;

void bootinfo_init(void) {
    const struct boot_info *src = (const struct boot_info *)PHYS_TO_VIRT(BOOTINFO_ADDR);
    if (src->magic != BOOTINFO_MAGIC) return;
    memcpy(&bi, src, sizeof bi);
    if (bi.mmap_count > MMAP_MAX) bi.mmap_count = MMAP_MAX;
    if (bi.flags & BI_MMAP) memcpy(mm, PHYS_TO_VIRT(bi.mmap_addr), bi.mmap_count * sizeof(struct mmap_entry));
    if (bi.flags & BI_FONT) memcpy(font, PHYS_TO_VIRT(bi.font_addr), sizeof font);
    valid = true;
}
bool bootinfo_valid(void) { return valid; }
const struct boot_info *bootinfo(void) { return valid ? &bi : NULL; }
const struct mmap_entry *bootinfo_mmap(int *count) {
    if (!valid || !(bi.flags & BI_MMAP)) { *count = 0; return NULL; }
    *count = (int)bi.mmap_count; return mm;
}
const uint8_t *bootinfo_font(void) { return (valid && (bi.flags & BI_FONT)) ? font : NULL; }
