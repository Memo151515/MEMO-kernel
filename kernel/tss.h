#ifndef TSS_H
#define TSS_H

#include <stdint.h>

void tss_install(int gdt_slot, uint16_t ss0, uint32_t esp0);
void tss_set_stack(uint32_t esp0);

#endif