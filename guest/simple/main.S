/* Deliberately tiny stand-alone guest image. It is not linked into Syle.
 * Prototype entry: long mode, private paging, RDI -> provisional boot record.
 * Only integer registers are used. RAX=1 writes RBX byte; RAX=2 exits RBX status. */
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
    xor %eax,%eax
    xor %ecx,%ecx
    cpuid
    cmp $1,%eax
    jne .Lbad
    cmp $0x656c7953,%ebx
    jne .Lbad
    lea greeting(%rip),%rsi
.Lprint:
    movzbl (%rsi),%ebx
    test %bl,%bl
    jz .Ldone
    mov $1,%eax
    vmcall
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
    .asciz "Hello from an independent Syle guest ELF!\n"
.section .bss
.balign 8
guest_bss:
    .skip 8
.section .note.GNU-stack,"",@progbits
