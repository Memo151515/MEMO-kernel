CC = x86_64-elf-gcc
AS = nasm
LD = x86_64-elf-ld

CFLAGS = -ffreestanding -m32 -fno-stack-protector -fno-pie -Wall -Wextra -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mgeneral-regs-only
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld -melf_i386

OBJS = boot/boot.o boot/gdt_flush.o boot/idt_flush.o boot/isr.o boot/irq.o boot/paging_enable.o boot/switch_to.o boot/usermode_jump.o kernel/kernel.o kernel/gdt.o kernel/idt.o kernel/isr.o kernel/irq.o kernel/keyboard.o kernel/paging.o kernel/kheap.o kernel/timer.o kernel/task.o kernel/syscall.o kernel/fs.o kernel/tss.o

all: memo_flat.bin

boot/boot.o: boot/boot.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/gdt_flush.o: boot/gdt_flush.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/idt_flush.o: boot/idt_flush.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/isr.o: boot/isr.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/irq.o: boot/irq.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/paging_enable.o: boot/paging_enable.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/switch_to.o: boot/switch_to.asm
	$(AS) $(ASFLAGS) $< -o $@

boot/usermode_jump.o: boot/usermode_jump.asm
	$(AS) $(ASFLAGS) $< -o $@

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/gdt.o: kernel/gdt.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/idt.o: kernel/idt.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/isr.o: kernel/isr.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/irq.o: kernel/irq.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/keyboard.o: kernel/keyboard.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/paging.o: kernel/paging.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/kheap.o: kernel/kheap.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/timer.o: kernel/timer.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/task.o: kernel/task.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/syscall.o: kernel/syscall.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/fs.o: kernel/fs.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/tss.o: kernel/tss.c
	$(CC) $(CFLAGS) -c $< -o $@

memo.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

memo_flat.bin: memo.bin
	x86_64-elf-objcopy -O binary memo.bin memo_flat.bin

clean:
	rm -f boot/*.o kernel/*.o memo.bin memo_flat.bin