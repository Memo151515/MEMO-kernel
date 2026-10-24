/* MNG/MEMO - 8259 PIC */
#include <memo/arch.h>

#define PIC1 0x20
#define PIC2 0xA0

void pic_init(void) {
    outb(PIC1, 0x11); io_wait(); outb(PIC2, 0x11); io_wait();
    outb(PIC1 + 1, 32); io_wait(); outb(PIC2 + 1, 40); io_wait();   /* vector offsets */
    outb(PIC1 + 1, 4);  io_wait(); outb(PIC2 + 1, 2);  io_wait();   /* cascade        */
    outb(PIC1 + 1, 1);  io_wait(); outb(PIC2 + 1, 1);  io_wait();   /* 8086 mode      */
    outb(PIC1 + 1, 0xFB);                                           /* only IRQ2 open */
    outb(PIC2 + 1, 0xFF);
}

void pic_unmask(int irq) {
    uint16_t port = irq < 8 ? PIC1 + 1 : PIC2 + 1;
    outb(port, inb(port) & ~(1 << (irq & 7)));
}

void pic_eoi(int irq) {
    if (irq >= 8) outb(PIC2, 0x20);
    outb(PIC1, 0x20);
}

bool pic_is_spurious(int irq) {
    if (irq != 7 && irq != 15) return false;
    uint16_t port = irq == 7 ? PIC1 : PIC2;
    outb(port, 0x0B);
    bool spurious = !(inb(port) & 0x80);
    if (spurious && irq == 15) outb(PIC1, 0x20);        /* master still needs EOI */
    return spurious;
}
