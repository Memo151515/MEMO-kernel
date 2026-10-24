/* MNG/MEMO - boot information handed over by MBoot (physical address 0x5000) */
#pragma once
#include <memo/kernel.h>

#define BOOTINFO_ADDR   0x5000
#define BOOTINFO_MAGIC  0x4D424F54u       /* 'MBOT' */
#define BI_FB    (1u << 0)
#define BI_FONT  (1u << 1)
#define BI_MMAP  (1u << 2)

struct boot_info {
    uint32_t magic, version, flags;
    uint32_t fb_addr, fb_pitch, fb_width, fb_height, fb_bpp;
    uint32_t mmap_count, mmap_addr, font_addr, boot_drive, vbe_mode;
} __packed;

struct mmap_entry {                         /* BIOS E820 */
    uint64_t base, len;
    uint32_t type, acpi;
} __packed;

#define MMAP_USABLE 1
#define MMAP_MAX    40

void  bootinfo_init(void);                  /* copies everything out of low memory */
bool  bootinfo_valid(void);
const struct boot_info  *bootinfo(void);
const struct mmap_entry *bootinfo_mmap(int *count);
const uint8_t *bootinfo_font(void);         /* 256 glyphs * 16 bytes, or NULL */
