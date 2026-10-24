/* MNG/MEMO - no-op replacements when an optional driver is configured out (see .config) */
#include <memo/kernel.h>
#include <memo/pci.h>
#include <memo/block.h>
#include <memo/rtc.h>

#ifndef CONFIG_PCI
int pci_count(void) { return 0; }
const struct pci_dev *pci_get(int i) { (void)i; return NULL; }
const struct pci_dev *pci_find_class(uint8_t c, uint8_t s) { (void)c; (void)s; return NULL; }
const char *pci_class_name(uint8_t c, uint8_t s) { (void)c; (void)s; return "?"; }
const char *pci_vendor_name(uint16_t v) { (void)v; return "?"; }
uint32_t pci_read32(uint8_t b, uint8_t d, uint8_t f, uint8_t o) { (void)b; (void)d; (void)f; (void)o; return 0xFFFFFFFF; }
void pci_write32(uint8_t b, uint8_t d, uint8_t f, uint8_t o, uint32_t v) { (void)b; (void)d; (void)f; (void)o; (void)v; }
#endif
#ifndef CONFIG_ATA
int ata_count(void) { return 0; }
bool ata_info(int i, char *m, size_t s, uint32_t *n, bool *p) { (void)i; (void)m; (void)s; (void)n; *p = false; return false; }
int ata_read(int i, uint32_t l, uint32_t c, void *b) { (void)i; (void)l; (void)c; (void)b; return -ENODEV; }
#endif
#ifndef CONFIG_RTC
void rtc_read(struct rtc_time *t) { memset(t, 0, sizeof *t); }
#endif
