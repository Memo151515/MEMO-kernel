/* MNG/MEMO - initcall runner + tiny driver registry */
#include <memo/arch.h>

extern initcall_t __initcall_start[], __initcall_end[];

void do_initcalls(void) {
    for (initcall_t *f = __initcall_start; f < __initcall_end; f++) (*f)();
}

#define MAX_DRV 32
static struct { const char *name, *desc, *status; } drv[MAX_DRV];
static int ndrv;

void driver_register(const char *name, const char *desc, const char *status) {
    if (ndrv < MAX_DRV) { drv[ndrv].name = name; drv[ndrv].desc = desc; drv[ndrv].status = status; ndrv++; }
}
int driver_count(void) { return ndrv; }
void driver_get(int i, const char **name, const char **desc, const char **status) {
    *name = drv[i].name; *desc = drv[i].desc; *status = drv[i].status;
}
