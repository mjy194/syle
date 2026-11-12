.text
.global ring3_kernel_switch
ring3_kernel_switch:
    cli
    mov %rdi, %cr3
    mov %rsi, %rsp
    xor %ebp, %ebp
    sub $8, %rsp
    movq $0, (%rsp)
    jmp *%rdx
.global ring3_enter
ring3_enter:
    /* rdi=entry, rsi=user stack, rdx=test, rcx=kernel address */
    mov $0x33, %ax
    mov %ax, %ds
    mov %ax, %es
    pushq $0x33
    push %rsi
    pushq $2                 /* IF=0, IOPL=0; no timer/scheduler yet */
    pushq $0x2b
    push %rdi
    mov %rdx, %rdi
    mov %rcx, %rsi
    xor %eax, %eax
    xor %ebx, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %ebp, %ebp
    xor %r8d, %r8d
    xor %r9d, %r9d
    xor %r10d, %r10d
    xor %r11d, %r11d
    xor %r12d, %r12d
    xor %r13d, %r13d
    xor %r14d, %r14d
    xor %r15d, %r15d
    iretq
.global ring3_syscall_entry
ring3_syscall_entry:
    pushq $0
    pushq $128
    cld
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp, %rdi
    mov %rsp, %r12
    and $-16, %rsp
    call ring3_syscall
    mov %r12, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    add $16, %rsp
    iretq
.section .note.GNU-stack,"",@progbits
