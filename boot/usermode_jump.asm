global usermode_jump

usermode_jump:
    mov eax, [esp+4]  
    mov ebx, [esp+8]  

    mov cx, 0x23      
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push 0x23         
    push ebx          
    pushf             
    push 0x1B         
    push eax          
    iret