/* MNG/MEMO - ATA PIO driver (legacy IDE ports, LBA28): identify + read sectors */
#include <memo/arch.h>
#include <memo/block.h>

struct ata_dev { bool present; uint16_t io; bool slave; char model[41]; uint32_t sectors; };
static struct ata_dev dev[4];

static bool wait_not_busy(uint16_t io) {
    for (int i = 0; i < 1000000; i++) if (!(inb(io + 7) & 0x80)) return true;
    return false;
}
static bool wait_drq(uint16_t io) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(io + 7);
        if (s & 0x01) return false;                  /* ERR */
        if (!(s & 0x80) && (s & 0x08)) return true;
    }
    return false;
}

static void identify(struct ata_dev *d) {
    uint16_t io = d->io;
    if (inb(io + 7) == 0xFF) return;                 /* floating bus */
    outb(io + 6, d->slave ? 0xB0 : 0xA0);
    for (int i = 0; i < 4; i++) inb(io + 0x206);     /* 400 ns delay */
    outb(io + 2, 0); outb(io + 3, 0); outb(io + 4, 0); outb(io + 5, 0);
    outb(io + 7, 0xEC);
    if (inb(io + 7) == 0) return;
    if (!wait_not_busy(io)) return;
    if (inb(io + 4) || inb(io + 5)) return;          /* ATAPI/SATA signature: skip */
    if (!wait_drq(io)) return;
    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(io);
    for (int i = 0; i < 20; i++) { d->model[i * 2] = (char)(id[27 + i] >> 8); d->model[i * 2 + 1] = (char)(id[27 + i] & 0xFF); }
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;
    d->sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    d->present = true;
}

int ata_count(void) { return 4; }
bool ata_info(int i, char *model, size_t msz, uint32_t *sectors, bool *present) {
    if (i < 0 || i > 3) return false;
    *present = dev[i].present;
    if (dev[i].present) { strlcpy(model, dev[i].model, msz); *sectors = dev[i].sectors; }
    return true;
}

int ata_read(int i, uint32_t lba, uint32_t count, void *buf) {
    if (i < 0 || i > 3 || !dev[i].present) return -ENODEV;
    struct ata_dev *d = &dev[i];
    uint16_t io = d->io; uint16_t *p = buf;
    if (lba + count > d->sectors || count == 0 || count > 255) return -EINVAL;
    uintptr_t fl = irq_save();
    if (!wait_not_busy(io)) { irq_restore(fl); return -EIO; }
    outb(io + 6, (uint8_t)(0xE0 | (d->slave ? 0x10 : 0) | ((lba >> 24) & 0x0F)));
    outb(io + 2, (uint8_t)count);
    outb(io + 3, (uint8_t)lba); outb(io + 4, (uint8_t)(lba >> 8)); outb(io + 5, (uint8_t)(lba >> 16));
    outb(io + 7, 0x20);
    for (uint32_t s = 0; s < count; s++) {
        if (!wait_drq(io)) { irq_restore(fl); return -EIO; }
        for (int w = 0; w < 256; w++) *p++ = inw(io);
    }
    irq_restore(fl);
    return 0;
}

static int ata_init(void) {
    dev[0].io = 0x1F0; dev[1].io = 0x1F0; dev[1].slave = true;
    dev[2].io = 0x170; dev[3].io = 0x170; dev[3].slave = true;
    int n = 0;
    for (int i = 0; i < 4; i++) { identify(&dev[i]); n += dev[i].present; }
    driver_register("ata", "ATA PIO (LBA28)", n ? "ok" : "no disks");
    return 0;
}
device_initcall(ata_init);
