/* MNG/MEMO - round-robin preemptive scheduler for kernel threads and user processes */
#include <memo/arch.h>
#include <memo/sched.h>
#include <memo/proc.h>

static task_t *current;
static uint32_t next_pid;

task_t *task_current(void) { return current; }

void sched_init(void) {
    task_t *idle = kcalloc(1, sizeof *idle);
    strlcpy(idle->name, "idle", sizeof idle->name);
    idle->pid = next_pid++;
    idle->state = TASK_RUNNING;
    idle->detached = true;
    idle->kstack_top = arch_boot_stack_top();
    idle->next = idle;
    current = idle;
}

static void task_exit_tramp(void) { task_exit(); }

task_t *task_alloc(const char *name, void (*entry)(void)) {
    task_t *t = kcalloc(1, sizeof *t);
    uint8_t *stack = kmalloc(KSTACK_SIZE);
    if (!t || !stack) { kfree(t); kfree(stack); return NULL; }
    strlcpy(t->name, name, sizeof t->name);
    t->kstack_base = (uintptr_t)stack;
    t->kstack_top = (uintptr_t)stack + KSTACK_SIZE;
    t->sp = arch_task_prepare_stack(t->kstack_top, entry, task_exit_tramp);
    t->state = TASK_READY;                    /* not in the run list until task_start() */
    t->detached = true;
    return t;
}

void task_start(task_t *t) {
    uintptr_t fl = irq_save();
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->next = current->next;
    current->next = t;
    irq_restore(fl);
}

task_t *task_create(const char *name, void (*entry)(void)) {
    task_t *t = task_alloc(name, entry);
    if (t) task_start(t);
    return t;
}

static void user_tramp(void) {
    task_t *t = current;
    arch_enter_usermode(t->user_entry, t->user_stack);
}

task_t *task_create_user(const char *name, uintptr_t entry, uintptr_t stack_top) {
    task_t *t = task_alloc(name, user_tramp);
    if (!t) return NULL;
    t->user = true; t->user_entry = entry; t->user_stack = stack_top;
    task_start(t);
    return t;
}

static bool runnable(task_t *t) {
    if (t->state == TASK_READY || t->state == TASK_RUNNING) return true;
    if (t->state == TASK_SLEEPING && (int32_t)(timer_ticks() - t->wake_tick) >= 0) { t->state = TASK_READY; return true; }
    return false;
}

void schedule(void) {
    uintptr_t fl = irq_save();
    task_t *prev = current, *next = prev->next;
    while (next != prev && !runnable(next)) next = next->next;
    if (next != prev) {
        if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
        next->state = TASK_RUNNING;
        next->runs++;
        current = next;
        arch_set_kernel_stack(next->kstack_top);
        arch_switch_hook(prev, next);
        arch_switch_to(&prev->sp, next->sp);
    }
    irq_restore(fl);
}

void task_yield(void) { schedule(); }

void task_sleep_ms(uint32_t ms) {
    uintptr_t fl = irq_save();
    current->wake_tick = timer_ticks() + (ms + 1000 / HZ - 1) / (1000 / HZ);
    current->state = TASK_SLEEPING;
    schedule();
    irq_restore(fl);
}

void task_exit(void) {
    cli();
    current->state = TASK_ZOMBIE;
    schedule();
    for (;;) hlt();                         /* never reached */
}

static void task_free(task_t *t) {
#ifdef __x86_64__
    if (t->mm) mm_destroy(t->mm);
#endif
    kfree(t->fds);
    kfree((void *)t->kstack_base);
    kfree(t);
}

int task_kill(uint32_t pid) {
    uintptr_t fl = irq_save();
    int rc = -ENOENT;
    task_t *t = current;
    do {
        if (t->pid == pid && t->state != TASK_ZOMBIE) {
            if (pid <= 1) rc = -EINVAL;
            else { rc = 0; t->exit_code = 137; if (t == current) { irq_restore(fl); task_exit(); } t->state = TASK_ZOMBIE; }
            break;
        }
        t = t->next;
    } while (t != current);
    irq_restore(fl);
    return rc;
}

int task_wait(uint32_t pid) {
    for (;;) {
        uintptr_t fl = irq_save();
        task_t *p = current; bool found = false;
        do {
            task_t *t = p->next;
            if (t->pid == pid && t != current) {
                found = true;
                if (t->state == TASK_ZOMBIE) {
                    int code = t->exit_code;
                    p->next = t->next;
                    irq_restore(fl);
                    task_free(t);
                    return code;
                }
                break;
            }
            p = t;
        } while (p != current);
        irq_restore(fl);
        if (!found) return -1;
        task_sleep_ms(20);
    }
}

void task_reap(void) {
    uintptr_t fl = irq_save();
    task_t *p = current;
    do {
        task_t *t = p->next;
        if (t != current && t->state == TASK_ZOMBIE && t->detached) {
            p->next = t->next;
            task_free(t);
        } else p = t;
    } while (p != current);
    irq_restore(fl);
}

int task_list(struct task_info *out, int max) {
    int n = 0;
    uintptr_t fl = irq_save();
    task_t *t = current;
    do {
        if (n < max) {
            out[n].pid = t->pid; strlcpy(out[n].name, t->name, sizeof out[n].name);
            out[n].state = t->state; out[n].runs = t->runs; out[n].user = t->user || t->mm;
            n++;
        }
        t = t->next;
    } while (t != current);
    irq_restore(fl);
    return n;
}
