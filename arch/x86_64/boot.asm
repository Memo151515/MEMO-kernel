; MNG/MEMO - x86_64 entry. MBoot jumps here in 32-bit protected mode at physical 0x100000.
; .boot code/data (page tables, stack) run at their physical addresses. We map
;   - physical 0..4 GiB at 0x0 (identity, only until the kernel is up; arch_mm_init drops it),
;   - physical 0..4 GiB at 0xFFFF800000000000 (direct map, PHYS_BASE),
;   - physical 0..1 GiB at 0xFFFFFFFF80000000 (kernel image),
; enable long mode and jump into the higher half.
extern kernel_main
extern __bss_start_phys
extern __bss_end_phys

section .boot exec progbits alloc
bits 32
global _start
_start:
    cli
    cld
    mov edi, __bss_start_phys
    mov ecx, __bss_end_phys
    sub ecx, edi
    shr ecx, 2
    xor eax, eax
    rep stosd                               ; clear .bss (the flat image has none)
    mov esp, boot_stack_top

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_lm
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_lm

    mov eax, boot_pdpt_lo
    or eax, 3
    mov [boot_pml4], eax
    mov eax, boot_pdpt_dm
    or eax, 3
    mov [boot_pml4 + 256*8], eax
    mov eax, boot_pdpt_hi
    or eax, 3
    mov [boot_pml4 + 511*8], eax
    mov eax, boot_pd
    or eax, 3
    mov [boot_pdpt_lo], eax
    mov [boot_pdpt_hi + 510*8], eax
    xor ecx, ecx
.dm:
    mov eax, ecx
    shl eax, 12
    add eax, boot_pd
    or eax, 3
    mov [boot_pdpt_dm + ecx*8], eax
    inc ecx
    cmp ecx, 4
    jne .dm
    xor ecx, ecx
.pd:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83                             ; present | rw | 2 MiB page
    mov [boot_pd + ecx*8], eax
    inc ecx
    cmp ecx, 2048
    jne .pd

    mov eax, cr4
    or eax, 1 << 5                           ; PAE
    mov cr4, eax
    mov eax, boot_pml4
    mov cr3, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8                           ; LME
    wrmsr
    mov eax, cr0
    or eax, 0x80010001                       ; PG | WP | PE
    mov cr0, eax
    lgdt [gdt64_ptr]
    jmp 0x08:long_entry

.no_lm:
    mov dword [0xB8000], 0x4F4F4F4E          ; "NO64"
    mov dword [0xB8004], 0x4F344F36
.hang32:
    hlt
    jmp .hang32

bits 64
long_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rax, long_high
    jmp rax                                  ; into the higher half

section .boot.data progbits alloc write align=4096
align 4096
global boot_pml4, boot_pdpt_lo, boot_pdpt_dm, boot_pdpt_hi, boot_pd, boot_stack_top
boot_pml4:    times 4096 db 0
boot_pdpt_lo: times 4096 db 0
boot_pdpt_dm: times 4096 db 0
boot_pdpt_hi: times 4096 db 0
boot_pd:      times 4096*4 db 0
align 16
boot_stack_bottom: times 16384 db 0
boot_stack_top:
align 8
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF                    ; 0x08 code64
    dq 0x00CF92000000FFFF                    ; 0x10 data
gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1
    dd gdt64

section .text
bits 64
long_high:
    mov rsp, 0xFFFF800000000000              ; PHYS_BASE
    add rsp, boot_stack_top                  ; the boot stack through the direct map
    xor ebp, ebp
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang
