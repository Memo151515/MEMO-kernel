; MNG/MEMO - i386 entry point. MBoot jumps here (protected mode, paging off) at 0x100000.
bits 32
extern kernel_main
extern __bss_start
extern __bss_end

section .boot exec progbits alloc
global _start
_start:
    cli
    cld
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    shr ecx, 2
    xor eax, eax
    rep stosd                   ; the flat binary has no .bss: clear it ourselves
    mov esp, boot_stack_top
    xor ebp, ebp
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
boot_stack_bottom: resb 16384
global boot_stack_top
boot_stack_top:
