/* Stand-alone integer-only guest: ordinary byte IN/OUT to virtual COM1.
 * Still uses the provisional ELF entry contract and VMCALL only to finish.
 * This is a device integration sample, not a lightweight operating system. */
.section .text
.global _start
.type _start,@function
_start:
    movabs $0x53474c4f41443031,%rax
    cmp %rax,0(%rdi)
    jne .Lbad
    cmpl $1,8(%rdi)
    jne .Lbad
    cmpl $40,12(%rdi)
    jne .Lbad
    cmpq $0x40000,16(%rdi)
    jne .Lbad
    cmp %rsp,24(%rdi)
    jne .Lbad
    lea _start(%rip),%rax
    cmp %rax,32(%rdi)
    jne .Lbad
    cmpq $0,guest_bss(%rip)
    jne .Lbad
    movq $0x1234,guest_bss(%rip)
    cmpq $0x1234,guest_bss(%rip)
    jne .Lbad

    /* Disable interrupts, set DLAB and a divisor, then use 8N1. */
    mov $0x3f9,%edx
    xor %eax,%eax
    out %al,%dx
    mov $0x3fb,%edx
    mov $0x83,%eax
    out %al,%dx
    mov $0x3f8,%edx
    mov $1,%eax
    out %al,%dx
    xor %eax,%eax
    in %dx,%al
    cmp $1,%al
    jne .Lbad
    mov $0x3f9,%edx
    xor %eax,%eax
    out %al,%dx
    mov $0x3fb,%edx
    mov $3,%eax
    out %al,%dx
    mov $0x3fa,%edx
    mov $0xc7,%eax
    out %al,%dx
    in %dx,%al
    cmp $0xc1,%al
    jne .Lbad
    mov $0x3fc,%edx
    mov $3,%eax
    out %al,%dx

    /* Scratch aliasing and preservation of the upper RAX bits on IN AL. */
    mov $0x3ff,%edx
    mov $0xa5,%eax
    out %al,%dx
    movabs $0x123456789abcde00,%rax
    in %dx,%al
    movabs $0x123456789abcdea5,%rcx
    cmp %rcx,%rax
    jne .Lbad
    mov $0x3f9,%edx
    in %dx,%al
    test %al,%al
    jne .Lbad

    lea greeting(%rip),%rsi
.Lprint:
    movzbl (%rsi),%ebx
    test %bl,%bl
    jz .Ldone
    mov $0x3fd,%edx
.Lready:
    in %dx,%al
    test $0x20,%al
    jz .Lready
    mov $0x3f8,%edx
    mov %bl,%al
    out %al,%dx
    inc %rsi
    jmp .Lprint
.Ldone:
    mov $2,%eax
    xor %ebx,%ebx
    vmcall
    ud2
.Lbad:
    mov $2,%eax
    mov $1,%ebx
    vmcall
    ud2
.size _start,.-_start
.section .rodata
greeting:
    .asciz "Hello via a virtual COM1 UART!\n"
.section .bss
.balign 8
guest_bss:
    .skip 8
.section .note.GNU-stack,"",@progbits
