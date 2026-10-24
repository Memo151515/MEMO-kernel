/* MNG/MEMO - kernel image layout. MBoot copies the flat binary to 1 MiB and jumps to its first byte. */
ENTRY(_start)
SECTIONS
{
    . = 1M;

    .text : ALIGN(4K) {
        *(.boot)                           /* entry code must come first */
        *(.text .text.*)
    }
    .rodata : ALIGN(4K) {
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
    .data : ALIGN(4K) { *(.data .data.*) }
    .user : ALIGN(4K) {                    /* the only memory ring 3 may touch */
        __user_start = .;
        *(.user.text) *(.user.data)
        . = ALIGN(4K);
        __user_end = .;
    }
    .bss : ALIGN(4K) {
        __bss_start = .;
        *(COMMON) *(.bss .bss.*)
        . = ALIGN(4K);
        __bss_end = .;
    }
    kernel_end = .;

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) *(.note.gnu.property) }
}
