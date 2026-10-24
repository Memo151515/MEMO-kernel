# MNG/MEMO kernel build system (Kbuild-lite)
#
#   make defconfig            default configuration (i386)      -> .config
#   make menuconfig           answer questions, edit .config
#   make                      build for ARCH from .config
#   make ARCH=x86_64          build the 64-bit kernel
#   make run                  build + boot in QEMU through MBoot
#   make clean | distclean
#
# CROSS defaults to x86_64-elf- (cross toolchain). On a normal Linux box you can use the host
# toolchain: make CROSS=

-include .config
ARCH   ?= i386
CROSS  ?= x86_64-elf-
MBOOT  ?= ../MEMO-mboot
CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
NASM   ?= nasm
BUILD   = build/$(ARCH)

ifeq ($(ARCH),i386)
  CARCH   = -m32
  NASMFMT = elf32
  LDEMU   = elf_i386
else ifeq ($(ARCH),x86_64)
  CARCH   = -m64 -mno-red-zone -mcmodel=kernel
  NASMFMT = elf64
  LDEMU   = elf_x86_64
else
  $(error Unknown ARCH '$(ARCH)' - use i386 or x86_64)
endif

ifeq ($(CONFIG_DEBUG),y)
  OPT = -O0 -g
else
  OPT = -O2
endif

CONFIG_OPTIONS := CONFIG_SERIAL CONFIG_PCI CONFIG_ATA CONFIG_RTC CONFIG_DEBUG
KEYMAP ?= us
CONFIG_DEFS := -DDEFAULT_KEYMAP=\"$(KEYMAP)\" $(foreach v,$(CONFIG_OPTIONS),$(if $(filter y,$($(v))),-D$(v)=1))

CFLAGS = $(CARCH) $(OPT) -std=gnu11 -Wall -Wextra -ffreestanding -fno-builtin -fno-stack-protector \
         -fno-pie -fno-pic -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns \
         -mgeneral-regs-only -MMD -MP \
         -Iinclude -Iarch/x86/include -Iarch/$(ARCH)/include $(CONFIG_DEFS)
LDFLAGS = -m $(LDEMU) -T arch/$(ARCH)/linker.ld -nostdlib -z noexecstack -z max-page-size=0x1000 --no-warn-rwx-segments

obj-y  := kernel/main.o kernel/printk.o kernel/bootinfo.o kernel/initcall.o kernel/irq.o \
          kernel/sched.o kernel/mm.o
ifeq ($(ARCH),x86_64)
obj-y  += kernel/pmm.o kernel/proc_stub.o kernel/proc/vm.o kernel/proc/elf.o kernel/proc/proc.o kernel/proc/linux.o shell/cmd_proc.o
else
obj-y  += kernel/syscall.o kernel/user_demo.o
endif
obj-y  += lib/string.o lib/vsprintf.o mm/heap.o fs/ramfs.o fs/ext2.o
obj-y  += drivers/console/console.o drivers/pic/pic.o drivers/timer/pit.o drivers/input/keyboard.o drivers/stubs.o
obj-y  += shell/shell.o shell/cmd_core.o shell/cmd_fs.o shell/cmd_sys.o
obj-y  += drivers/serial/serial.o kernel/net/net.o drivers/net/e1000.o shell/cmd_net.o
obj-$(CONFIG_PCI) += drivers/pci/pci.o
obj-$(CONFIG_ATA) += drivers/block/ata.o
obj-$(CONFIG_RTC) += drivers/rtc/rtc.o
arch-y := arch/$(ARCH)/boot.o arch/$(ARCH)/stubs.o arch/$(ARCH)/cpu.o arch/$(ARCH)/mm.o

OBJS = $(addprefix $(BUILD)/,$(arch-y) $(obj-y))
ELF  = $(BUILD)/memo-$(ARCH).elf
FLAT = $(BUILD)/memo_flat.bin
Q    = $(if $(V),,@)

all: $(FLAT)

$(BUILD)/%.o: %.c
	$(Q)mkdir -p $(dir $@)
	@echo "  CC   $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	$(Q)mkdir -p $(dir $@)
	@echo "  NASM $<"
	$(Q)$(NASM) -f $(NASMFMT) $< -o $@

$(ELF): $(OBJS) arch/$(ARCH)/linker.ld
	@echo "  LD   $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(FLAT): $(ELF)
	@echo "  BIN  $@"
	$(Q)$(OBJCOPY) -O binary $< $@
	$(Q)size=$$(wc -c < $@); echo "  $(MEMO_NAME) $(ARCH): $$size bytes"; \
	  if [ $$size -gt 524288 ]; then echo "ERROR: kernel is bigger than the 512 KiB MBoot loads"; rm -f $@; exit 1; fi

defconfig:
	cp configs/defconfig .config
x86_64_defconfig:
	cp configs/x86_64_defconfig .config
menuconfig:
	@./scripts/menuconfig.sh

install-mboot: $(FLAT)
	cp $(FLAT) $(MBOOT)/memo_flat.bin

image: $(FLAT)
	$(MBOOT)/mkdisk.sh $(FLAT) $(BUILD)/disk.img

rootfs:
	./tools/mkrootfs.sh

run: image
	qemu-system-x86_64 -drive file=$(BUILD)/disk.img,format=raw,if=ide,index=0 \
	  $(if $(wildcard build/rootfs.ext4),-drive file=build/rootfs.ext4,format=raw,if=ide,index=1) \
	  -m 256 -vga std -serial stdio -nic user,model=e1000

clean:
	rm -rf build
distclean: clean
	rm -f .config

.PHONY: rootfs all defconfig x86_64_defconfig menuconfig install-mboot image run clean distclean
-include $(OBJS:.o=.d)
