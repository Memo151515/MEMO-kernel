/* MNG/MEMO - Intel PRO/1000 (e1000, 82540/82545/82574) Ethernet driver. x86_64 only for now. */
#include <memo/arch.h>
#include <memo/net.h>
#include <memo/pci.h>

#ifdef __x86_64__
#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_ICR    0x00C0
#define REG_IMC    0x00D8
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400
#define REG_TIPG   0x0410
#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818
#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818
#define REG_MTA    0x5200
#define REG_RAL    0x5400
#define REG_RAH    0x5404

#define NRX 32
#define NTX 16
#define BUFSZ 2048
struct desc { uint64_t addr; uint16_t length, csum; uint8_t status, errors; uint16_t special; } __packed;   /* RX */
struct tdesc { uint64_t addr; uint16_t length; uint8_t cso, cmd, status, css; uint16_t special; } __packed;

static volatile uint8_t *mmio;
static struct desc *rx; static struct tdesc *tx;
static uint8_t *rxbuf, *txbuf;
static uint32_t rx_cur, tx_cur;
static struct netdev nic = { .name = "eth0" };

static inline uint32_t rd(uint32_t r) { return *(volatile uint32_t *)(mmio + r); }
static inline void wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

static int e1000_send(const void *frame, size_t len) {
    if (len > BUFSZ) return -EINVAL;
    uintptr_t fl = irq_save();
    struct tdesc *d = &tx[tx_cur];
    for (int i = 0; i < 100000 && tx_cur != 0xFFFF && !(d->status & 1) && d->cmd; i++) { }   /* wait for DD of old use */
    memcpy(txbuf + (size_t)tx_cur * BUFSZ, frame, len);
    d->addr = VIRT_TO_PHYS(txbuf + (size_t)tx_cur * BUFSZ);
    d->length = (uint16_t)len; d->cso = 0; d->cmd = 0x0B; d->status = 0; d->css = 0; d->special = 0;   /* EOP | IFCS | RS */
    tx_cur = (tx_cur + 1) % NTX;
    wr(REG_TDT, tx_cur);
    irq_restore(fl);
    return 0;
}

static void e1000_poll(void) {
    while (rx[rx_cur].status & 1) {
        uint16_t len = rx[rx_cur].length;
        if (len > 0 && len <= BUFSZ && !rx[rx_cur].errors) net_rx(rxbuf + (size_t)rx_cur * BUFSZ, len);
        rx[rx_cur].status = 0;
        wr(REG_RDT, rx_cur);
        rx_cur = (rx_cur + 1) % NRX;
    }
}

static int e1000_init(void) {
    static const uint16_t ids[] = { 0x100E, 0x100F, 0x1004, 0x1008, 0x10D3, 0x153A, 0x1503 };
    const struct pci_dev *p = NULL;
    for (int i = 0; i < pci_count() && !p; i++) {
        const struct pci_dev *d = pci_get(i);
        if (d->vendor != 0x8086) continue;
        for (size_t k = 0; k < ARRAY_SIZE(ids); k++) if (d->device == ids[k]) p = d;
    }
    if (!p) return 0;
    uint32_t cmd = pci_read32(p->bus, p->dev, p->fn, 4) & 0xFFFF;
    pci_write32(p->bus, p->dev, p->fn, 4, cmd | 0x7);                 /* IO + memory + bus master */
    uint64_t bar = p->bar[0] & ~0xFu;
    arch_map_mmio(bar, 0x20000);
    mmio = PHYS_TO_VIRT(bar);

    wr(REG_IMC, 0xFFFFFFFF);                                          /* no interrupts: we poll */
    wr(REG_CTRL, rd(REG_CTRL) | (1u << 26));                          /* reset */
    for (int i = 0; i < 100000 && (rd(REG_CTRL) & (1u << 26)); i++) { }
    wr(REG_IMC, 0xFFFFFFFF);
    wr(REG_CTRL, (rd(REG_CTRL) | (1u << 6) | (1u << 5)) & ~(1u << 3) & ~(1u << 31));   /* SLU, ASDE */
    for (int i = 0; i < 128; i++) wr(REG_MTA + (uint32_t)i * 4, 0);

    uint32_t ral = rd(REG_RAL), rah = rd(REG_RAH);
    nic.mac[0] = (uint8_t)ral; nic.mac[1] = (uint8_t)(ral >> 8); nic.mac[2] = (uint8_t)(ral >> 16); nic.mac[3] = (uint8_t)(ral >> 24);
    nic.mac[4] = (uint8_t)rah; nic.mac[5] = (uint8_t)(rah >> 8);

    rx = kcalloc(NRX, sizeof *rx); tx = kcalloc(NTX, sizeof *tx);
    rxbuf = kmalloc((size_t)NRX * BUFSZ); txbuf = kmalloc((size_t)NTX * BUFSZ);
    if (!rx || !tx || !rxbuf || !txbuf) { driver_register("e1000", "Intel PRO/1000", "no memory"); return 0; }
    for (int i = 0; i < NRX; i++) rx[i].addr = VIRT_TO_PHYS(rxbuf + (size_t)i * BUFSZ);
    uint64_t rp = VIRT_TO_PHYS(rx), tp = VIRT_TO_PHYS(tx);
    wr(REG_RDBAL, (uint32_t)rp); wr(REG_RDBAH, (uint32_t)(rp >> 32)); wr(REG_RDLEN, NRX * sizeof(struct desc));
    wr(REG_RDH, 0); wr(REG_RDT, NRX - 1);
    wr(REG_RCTL, (1u << 1) | (1u << 15) | (1u << 26));                /* EN, BAM, SECRC (2048-byte buffers) */
    wr(REG_TDBAL, (uint32_t)tp); wr(REG_TDBAH, (uint32_t)(tp >> 32)); wr(REG_TDLEN, NTX * sizeof(struct tdesc));
    wr(REG_TDH, 0); wr(REG_TDT, 0);
    wr(REG_TIPG, 0x0060200A);
    wr(REG_TCTL, (1u << 1) | (1u << 3) | (0x10u << 4) | (0x40u << 12));   /* EN, PSP, CT, COLD */

    nic.send = e1000_send; nic.poll = e1000_poll;
    netdev_register(&nic);
    driver_register("e1000", "Intel PRO/1000 Ethernet (polling)", "ok");
    return 0;
}
late_initcall(e1000_init);
#endif
