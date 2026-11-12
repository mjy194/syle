/* Two independently identified EPT12 contexts, constructed by L1. */
    mov $0x10000,%edi
    mov $(8*4096/8),%ecx
    xor %eax,%eax
.Lnested_context_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_context_zero
    movq $0x11007,0x10000
    movq $0x12007,0x11000
    movq $0x13007,0x12000
    movq $0x15007,0x14000
    movq $0x16007,0x15000
    movq $0x17007,0x16000
    mov $0x13000,%ebx
    mov $0x17000,%edi
    mov $1,%ecx
.Lnested_context_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax
    mov %rax,(%rbx,%rcx,8)
    mov %rax,(%rdi,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_context_identity
    movq $0x6035,48(%rbx)
    movq $0x6035,48(%rdi)
    movq $0x8033,120(%rbx) /* A:f000 -> backing with 11 */
    movq $0x9033,120(%rdi) /* B:f000 -> backing with 22 */
    movq $0x1001e,0xc830
    movq $0,0xc838
