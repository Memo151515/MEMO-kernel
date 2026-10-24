/* MNG/MEMO x86_64 - higher-half kernel. MBoot copies the flat image to physical 1 MiB.
 * .boot (entry code, boot page tables, boot stack) runs at its physical address; everything
 * else is linked at 0xFFFFFFFF80000000 + physical and loaded (AT) right behind .boot. */
ENTRY(_start)
KERNEL_VMA = 0xFFFFFFFF80000000;
SECTIONS
{
    . = 1M;
    .boot : { *(.boot) *(.boot.data) }
    . = ALIGN(4K) + KERNEL_VMA;

    .text : AT(ADDR(.text) - KERNEL_VMA) { *(.text .text.*) }
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) ALIGN(4K) {
        *(.rodata .rodata.*)
        . = ALIGN(8);
        __initcall_start = .;
        KEEP(*(.initcall.1)) KEEP(*(.initcall.2)) KEEP(*(.initcall.3))
        __initcall_end = .;
        . = ALIGN(8);
        __shellcmd_start = .;
        KEEP(*(.shellcmd))
        __shellcmd_end = .;
    }
    .data : AT(ADDR(.data) - KERNEL_VMA) ALIGN(4K) { *(.data .data.*) }
    .bss : AT(ADDR(.bss) - KERNEL_VMA) ALIGN(4K) {
        __bss_start = .;
        *(COMMON) *(.bss .bss.*)
        . = ALIGN(4K);
        __bss_end = .;
    }
    kernel_end = .;
    __bss_start_phys = __bss_start - KERNEL_VMA;
    __bss_end_phys   = __bss_end - KERNEL_VMA;

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) *(.note.gnu.property) }
}
