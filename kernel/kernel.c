#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "keyboard.h"
#include "paging.h"
#include "kheap.h"
#include "timer.h"
#include "task.h"
#include "syscall.h"
#include "fs.h"
#include "tss.h"

static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

extern uint32_t kernel_end;

void clear_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = (uint16_t) ' ' | (uint16_t) 0x0F00;
    }
}

static void print_at_row(const char* label, uint32_t value, int row) {
    int pos = row * VGA_WIDTH;
    for (int i = 0; label[i] != '\0'; i++) {
        VGA_MEMORY[pos++] = (uint16_t) label[i] | (uint16_t) 0x0A00;
    }

    char buf[16];
    int i = 0;
    if (value == 0) {
        buf[i++] = '0';
    } else {
        char tmp[16];
        int j = 0;
        while (value > 0) {
            tmp[j++] = '0' + (value % 10);
            value /= 10;
        }
        while (j > 0) buf[i++] = tmp[--j];
    }
    buf[i] = '\0';

    for (int k = 0; buf[k] != '\0'; k++) {
        VGA_MEMORY[pos++] = (uint16_t) buf[k] | (uint16_t) 0x0A00;
    }
    while (pos < (row + 1) * VGA_WIDTH) {
        VGA_MEMORY[pos++] = (uint16_t) ' ' | (uint16_t) 0x0A00;
    }
}

static void task1_entry(void) {
    uint32_t counter = 0;
    for (;;) {
        print_at_row("Task 1: ", counter++, 18);
        for (volatile int i = 0; i < 3000000; i++);
    }
}

static void task2_entry(void) {
    uint32_t counter = 0;
    for (;;) {
        print_at_row("Task 2: ", counter++, 19);
        for (volatile int i = 0; i < 3000000; i++);
    }
}

static void user_program(void) {
    const char* msg = "Hello from RING 3 (userspace)!";
    __asm__ volatile (
        "mov %0, %%ebx\n\t"
        "mov $1, %%eax\n\t"
        "int $0x80"
        :
        : "r" (msg)
        : "eax", "ebx"
    );

    for (;;) {
    }
}

void kernel_main(void) {
    clear_screen();
    gdt_install();
    idt_install();
    irq_install();
    keyboard_install();
    paging_install();
    kheap_init((uint32_t)&kernel_end);
    timer_install();
    syscall_install();
    fs_install();

    tss_install(5, 0x10, 0);

    uint8_t* kernel_stack_for_tss = (uint8_t*) kmalloc(4096);
    tss_set_stack((uint32_t)(kernel_stack_for_tss + 4096));

    const char* str = "MEMO booting...";
    for (size_t i = 0; str[i] != '\0'; i++) {
        VGA_MEMORY[i] = (uint16_t) str[i] | (uint16_t) 0x0F00;
    }

    tasking_install();
    task_create(task1_entry);
    task_create(task2_entry);

    __asm__ volatile ("sti");

    jump_to_usermode(user_program);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}