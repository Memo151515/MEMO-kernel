; MNG/MEMO - i386 interrupt stubs, context switch, ring-3 entry
bits 32
extern interrupt_dispatch

section .text

%macro ISR_NOERR 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro
%macro ISR_ERR 1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

%assign i 0
%rep 48
  %if (i == 8) || (i >= 10 && i <= 14) || (i == 17) || (i == 21) || (i == 29) || (i == 30)
    ISR_ERR %[i]
  %else
    ISR_NOERR %[i]
  %endif
  %assign i i+1
%endrep
ISR_NOERR 128

isr_common:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp                    ; struct regs *
    call interrupt_dispatch
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    iretd

; void arch_switch_to(uintptr_t *old_sp, uintptr_t new_sp)
global arch_switch_to
arch_switch_to:
    mov eax, [esp+4]
    mov edx, [esp+8]
    pushfd
    push ebp
    push ebx
    push esi
    push edi
    mov [eax], esp
    mov esp, edx
    pop edi
    pop esi
    pop ebx
    pop ebp
    popfd
    ret

; void arch_enter_usermode(uintptr_t entry, uintptr_t user_sp)
global arch_enter_usermode
arch_enter_usermode:
    mov eax, [esp+4]
    mov ebx, [esp+8]
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    push dword 0x23             ; ss
    push ebx                    ; esp
    pushfd
    pop edx
    or edx, 0x200               ; IF=1
    push edx
    push dword 0x1B             ; cs (ring 3)
    push eax                    ; eip
    iretd

section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dd isr%[i]
  %assign i i+1
%endrep
global isr_stub_128
isr_stub_128: dd isr128
