#include "task.h"
#include "kheap.h"

#define STACK_SIZE 4096

extern void switch_to(uint32_t* old_esp_store, uint32_t new_esp);
extern void usermode_jump(uint32_t entry, uint32_t user_stack);

static task_t* current_task = 0;
static task_t* task_list_head = 0;
static task_t* task_list_tail = 0;
static int tasking_enabled = 0;

static uint32_t* push(uint32_t* sp, uint32_t val) {
    sp--;
    *sp = val;
    return sp;
}

static void list_append(task_t* t) {
    if (task_list_head == 0) {
        task_list_head = t;
        task_list_tail = t;
        t->next = t;
    } else {
        t->next = task_list_head;
        task_list_tail->next = t;
        task_list_tail = t;
    }
}

void task_create(void (*entry)(void)) {
    task_t* t = (task_t*) kmalloc(sizeof(task_t));
    uint8_t* stack_mem = (uint8_t*) kmalloc(STACK_SIZE);
    uint32_t* sp = (uint32_t*)(stack_mem + STACK_SIZE);

    sp = push(sp, (uint32_t) entry);
    sp = push(sp, 0x202);
    sp = push(sp, 0);
    sp = push(sp, 0);
    sp = push(sp, 0);
    sp = push(sp, 0);
    sp = push(sp, 0);
    sp = push(sp, 0);
    sp = push(sp, 0);
    sp = push(sp, 0);

    t->esp = (uint32_t) sp;
    list_append(t);
}

void tasking_install(void) {
    task_t* kernel_task = (task_t*) kmalloc(sizeof(task_t));
    kernel_task->esp = 0;
    list_append(kernel_task);

    current_task = kernel_task;
    tasking_enabled = 1;
}

void schedule(void) {
    if (!tasking_enabled || current_task == 0) return;

    task_t* prev = current_task;
    current_task = current_task->next;

    switch_to(&prev->esp, current_task->esp);
}

void jump_to_usermode(void (*entry)(void)) {
    uint8_t* user_stack_mem = (uint8_t*) kmalloc(4096);
    uint32_t user_stack_top = (uint32_t)(user_stack_mem + 4096);

    usermode_jump((uint32_t) entry, user_stack_top);
}