#pragma once
#include <memo/kernel.h>
struct pci_dev {
    uint8_t bus, dev, fn;
    uint16_t vendor, device;
    uint8_t class_code, subclass, prog_if, irq;
    uint32_t bar[6];
};
int  pci_count(void);
const struct pci_dev *pci_get(int i);
const struct pci_dev *pci_find_class(uint8_t cls, uint8_t sub);
const char *pci_class_name(uint8_t cls, uint8_t sub);
const char *pci_vendor_name(uint16_t vendor);
uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);
void pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t v);
