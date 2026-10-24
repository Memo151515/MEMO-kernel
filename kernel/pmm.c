/* MNG/MEMO - physical frame allocator (bitmap) */
#include <memo/arch.h>
#include <memo/mm.h>

#define MAX_FRAMES (1u << 20)                    /* 4 GiB */
static uint32_t bitmap[MAX_FRAMES / 32];         /* 1 = used */
static uint64_t first_frame, nframes, free_frames, hint;

static inline bool used(uint64_t f) { return bitmap[f / 32] & (1u << (f % 32)); }

void pmm_init(uint64_t lo, uint64_t hi) {
    memset(bitmap, 0xFF, sizeof bitmap);
    first_frame = ALIGN_UP(lo, 4096ULL) / 4096;
    uint64_t end = hi / 4096;
    if (end > MAX_FRAMES) end = MAX_FRAMES;
    nframes = end > first_frame ? end - first_frame : 0;
    for (uint64_t f = first_frame; f < end; f++) bitmap[f / 32] &= ~(1u << (f % 32));
    free_frames = nframes;
    hint = first_frame;
}

uint64_t pmm_alloc(void) {
    uintptr_t fl = irq_save();
    uint64_t end = first_frame + nframes;
    for (uint64_t i = 0; i < nframes; i++) {
        uint64_t f = hint + i; if (f >= end) f -= nframes;
        if (!used(f)) {
            bitmap[f / 32] |= 1u << (f % 32);
            free_frames--; hint = f + 1;
            irq_restore(fl);
            memset(PHYS_TO_VIRT(f * 4096), 0, 4096);
            return f * 4096;
        }
    }
    irq_restore(fl);
    return 0;
}

void pmm_free(uint64_t phys) {
    uint64_t f = phys / 4096;
    uintptr_t fl = irq_save();
    if (f >= first_frame && f < first_frame + nframes && used(f)) { bitmap[f / 32] &= ~(1u << (f % 32)); free_frames++; }
    irq_restore(fl);
}
uint64_t pmm_free_frames(void) { return free_frames; }
uint64_t pmm_total_frames(void) { return nframes; }
