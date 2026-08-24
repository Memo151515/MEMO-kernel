#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef struct task {
    uint32_t esp;
    struct task* next;
} task_t;

void tasking_install(void);
void task_create(void (*entry)(void));
void schedule(void);
void jump_to_usermode(void (*entry)(void));

#endif