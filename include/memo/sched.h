#pragma once
#include <memo/kernel.h>

enum task_state { TASK_READY, TASK_RUNNING, TASK_SLEEPING, TASK_ZOMBIE };
#define KSTACK_SIZE 16384

struct mm;
struct file;

typedef struct task {
    uint8_t  fpu[512] __attribute__((aligned(16)));   /* fxsave area (processes only) */
    uint32_t pid;
    char     name[16];
    enum task_state state;
    uintptr_t sp;                     /* saved kernel stack pointer */
    uintptr_t kstack_base, kstack_top;
    struct task *next;                /* circular list */
    uint32_t wake_tick;
    uint32_t runs;                    /* times scheduled */
    bool     user;
    bool     detached;                /* reaped automatically (kernel threads); processes are reaped by task_wait */
    uintptr_t user_entry, user_stack;
    struct mm   *mm;                  /* address space (NULL = kernel thread) */
    struct file *fds;                 /* file table of a process */
    uint64_t fs_base;                 /* TLS pointer (arch_prctl) */
    int      exit_code;
} task_t;

struct task_info { uint32_t pid; char name[16]; enum task_state state; uint32_t runs; bool user; };

void    sched_init(void);
task_t *task_alloc(const char *name, void (*entry)(void));   /* not runnable until task_start() */
void    task_start(task_t *t);
task_t *task_create(const char *name, void (*entry)(void));
task_t *task_create_user(const char *name, uintptr_t entry, uintptr_t stack_top);
task_t *task_current(void);
void    task_exit(void) __noreturn;
void    task_sleep_ms(uint32_t ms);
void    task_yield(void);
int     task_kill(uint32_t pid);
int     task_wait(uint32_t pid);             /* blocks until the process ends; returns its exit code */
void    task_reap(void);
void    schedule(void);
int     task_list(struct task_info *out, int max);

/* time */
uint32_t timer_ticks(void);
uint32_t uptime_ms(void);

/* native MEMO syscall numbers (int 0x80, i386) */
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_GETPID  3
#define SYS_UPTIME  4
#define SYS_SLEEP   5
#define SYS_YIELD   6

/* user demo (kernel/user_demo.c, i386 only) */
bool user_demo_start(bool fault);
