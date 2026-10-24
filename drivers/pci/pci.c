/* MNG/MEMO - PCI bus enumeration (configuration mechanism #1) */
#include <memo/arch.h>
#include <memo/pci.h>

#define MAX_PCI 64
static struct pci_dev devs[MAX_PCI];
static int ndev;

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(0xCF8, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
void pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t v) {
    outl(0xCF8, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}

static void probe(uint8_t bus, uint8_t dev, uint8_t fn) {
    uint32_t id = pci_read32(bus, dev, fn, 0);
    if ((id & 0xFFFF) == 0xFFFF || ndev >= MAX_PCI) return;
    struct pci_dev *d = &devs[ndev++];
    d->bus = bus; d->dev = dev; d->fn = fn;
    d->vendor = (uint16_t)(id & 0xFFFF); d->device = (uint16_t)(id >> 16);
    uint32_t cls = pci_read32(bus, dev, fn, 8);
    d->class_code = (uint8_t)(cls >> 24); d->subclass = (uint8_t)(cls >> 16); d->prog_if = (uint8_t)(cls >> 8);
    d->irq = (uint8_t)(pci_read32(bus, dev, fn, 0x3C) & 0xFF);
    for (int i = 0; i < 6; i++) d->bar[i] = pci_read32(bus, dev, fn, (uint8_t)(0x10 + i * 4));
}

int pci_count(void) { return ndev; }
const struct pci_dev *pci_get(int i) { return (i >= 0 && i < ndev) ? &devs[i] : NULL; }
const struct pci_dev *pci_find_class(uint8_t cls, uint8_t sub) {
    for (int i = 0; i < ndev; i++) if (devs[i].class_code == cls && devs[i].subclass == sub) return &devs[i];
    return NULL;
}

const char *pci_class_name(uint8_t c, uint8_t s) {
    switch (c) {
    case 0x00: return "Unclassified";
    case 0x01: return s == 1 ? "IDE controller" : s == 6 ? "SATA controller" : s == 8 ? "NVMe controller" : "Storage controller";
    case 0x02: return s == 0 ? "Ethernet controller" : "Network controller";
    case 0x03: return s == 0 ? "VGA controller" : "Display controller";
    case 0x04: return s == 3 ? "Audio device" : "Multimedia controller";
    case 0x05: return "Memory controller";
    case 0x06: return s == 0 ? "Host bridge" : s == 1 ? "ISA bridge" : s == 4 ? "PCI bridge" : "Bridge";
    case 0x07: return "Communication controller";
    case 0x08: return "System peripheral";
    case 0x09: return "Input device";
    case 0x0C: return s == 3 ? "USB controller" : "Serial bus controller";
    case 0x0D: return "Wireless controller";
    default:   return "Other";
    }
}
const char *pci_vendor_name(uint16_t v) {
    switch (v) {
    case 0x8086: return "Intel";       case 0x1022: return "AMD";
    case 0x10DE: return "NVIDIA";      case 0x1002: return "AMD/ATI";
    case 0x1AF4: return "Red Hat/virtio"; case 0x1234: return "QEMU/Bochs";
    case 0x15AD: return "VMware";      case 0x80EE: return "VirtualBox";
    case 0x10EC: return "Realtek";     case 0x14E4: return "Broadcom";
    case 0x168C: return "Qualcomm Atheros"; case 0x1B36: return "Red Hat (QEMU)";
    default: return "Unknown";
    }
}

static int pci_init(void) {
    for (int bus = 0; bus < 8; bus++)
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;
            probe((uint8_t)bus, (uint8_t)dev, 0);
            if (pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0x0C) & 0x00800000)       /* multifunction */
                for (int fn = 1; fn < 8; fn++) probe((uint8_t)bus, (uint8_t)dev, (uint8_t)fn);
        }
    driver_register("pci", "PCI bus enumeration", ndev ? "ok" : "no devices");
    return 0;
}
core_initcall(pci_init);
