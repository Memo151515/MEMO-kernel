/* MNG/MEMO - kernel heap: first-fit free list with splitting and coalescing */
#include <memo/arch.h>

#define HEAP_MAGIC 0xC0FFEE42u
struct block {
    uint32_t magic;
    uint32_t free;
    size_t   size;              /* payload size */
    struct block *next, *prev;  /* address ordered */
};
#define HDR ALIGN_UP(sizeof(struct block), 16)

static struct block *head;
static size_t heap_total;

void kheap_init(uintptr_t start, uintptr_t end) {
    start = ALIGN_UP(start, 16); end = ALIGN_DOWN(end, 16);
    head = (struct block *)start;
    head->magic = HEAP_MAGIC; head->free = 1;
    head->size = (end - start) - HDR;
    head->next = head->prev = NULL;
    heap_total = end - start;
}

void *kmalloc(size_t n) {
    if (!n) n = 1;
    n = ALIGN_UP(n, 16);
    uintptr_t fl = irq_save();
    for (struct block *b = head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + HDR + 32) {            /* split */
            struct block *nb = (struct block *)((uint8_t *)b + HDR + n);
            nb->magic = HEAP_MAGIC; nb->free = 1;
            nb->size = b->size - n - HDR;
            nb->next = b->next; nb->prev = b;
            if (b->next) b->next->prev = nb;
            b->next = nb; b->size = n;
        }
        b->free = 0;
        irq_restore(fl);
        return (uint8_t *)b + HDR;
    }
    irq_restore(fl);
    return NULL;
}

void kfree(void *p) {
    if (!p) return;
    struct block *b = (struct block *)((uint8_t *)p - HDR);
    if (b->magic != HEAP_MAGIC) panic("kfree: bad pointer %p", p);
    uintptr_t fl = irq_save();
    if (b->free) panic("kfree: double free %p", p);
    b->free = 1;
    if (b->next && b->next->free) {                       /* merge with next */
        b->size += HDR + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {                       /* merge with prev */
        b->prev->size += HDR + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
    irq_restore(fl);
}

void *kcalloc(size_t n, size_t sz) {
    void *p = kmalloc(n * sz);
    if (p) memset(p, 0, n * sz);
    return p;
}

void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    struct block *b = (struct block *)((uint8_t *)p - HDR);
    if (b->size >= n) return p;
    void *np = kmalloc(n);
    if (!np) return NULL;
    memcpy(np, p, b->size);
    kfree(p);
    return np;
}

void kheap_stats(size_t *total, size_t *used, size_t *nblocks) {
    size_t u = 0, nb = 0;
    uintptr_t fl = irq_save();
    for (struct block *b = head; b; b = b->next) { nb++; if (!b->free) u += b->size + HDR; }
    irq_restore(fl);
    if (total) *total = heap_total;
    if (used) *used = u;
    if (nblocks) *nblocks = nb;
}
