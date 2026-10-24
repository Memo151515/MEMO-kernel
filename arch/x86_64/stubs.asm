; MNG/MEMO - x86_64 interrupt stubs, context switch, ring-3 entry
bits 64
extern interrupt_dispatch

section .text

%macro ISR_NOERR 1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro
%macro ISR_ERR 1
isr%1:
    push qword %1
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
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp                ; struct regs *
    cld
    call interrupt_dispatch
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
    iretq

; void arch_switch_to(uintptr_t *old_sp /*rdi*/, uintptr_t new_sp /*rsi*/)
global arch_switch_to
arch_switch_to:
    pushfq
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    popfq
    ret

; void arch_enter_usermode(uintptr_t entry /*rdi*/, uintptr_t user_sp /*rsi*/)
global arch_enter_usermode
arch_enter_usermode:
    push qword 0x1B             ; ss (user data)
    push rsi                    ; rsp
    pushfq
    pop rax
    or rax, 0x200               ; IF=1
    push rax
    push qword 0x23             ; cs (user code64)
    push rdi                    ; rip
    iretq

; SYSCALL entry (Linux ABI): rax=nr rdi rsi rdx r10 r8 r9; rcx=user rip, r11=user rflags.
; We build the same frame as an interrupt so struct regs / iretq work for the return.
extern linux_syscall
global syscall_entry
syscall_entry:
    mov [rel syscall_user_rsp], rsp
    mov rsp, [rel syscall_kstack]
    push qword 0x1B             ; ss
    push qword [rel syscall_user_rsp]
    push r11                    ; rflags
    push qword 0x23             ; cs
    push rcx                    ; rip
    push qword 0                ; err_code
    push qword 0x100            ; int_no: marks a SYSCALL entry
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    cld
    call linux_syscall
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
    iretq

extern syscall_kstack
section .bss
syscall_user_rsp: resq 1

section .rodata
align 8
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dq isr%[i]
  %assign i i+1
%endrep
global isr_stub_128
isr_stub_128: dq isr128
