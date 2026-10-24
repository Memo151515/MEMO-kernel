#pragma once
#include <memo/kernel.h>
int  ata_count(void);
bool ata_info(int i, char *model, size_t msz, uint32_t *sectors, bool *present);
int  ata_read(int i, uint32_t lba, uint32_t count, void *buf);
