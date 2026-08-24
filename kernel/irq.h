#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>
#include "isr.h"

typedef void (*isr_t)(registers_t);

void irq_install(void);
void irq_install_handler(int irq, isr_t handler);
void irq_uninstall_handler(int irq);

#endif