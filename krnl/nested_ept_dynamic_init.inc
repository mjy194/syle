/* Eight private four-level EPT12 contexts: 32 table pages total. */
    mov $0x10000,%edi
    mov $(32*4096/8),%ecx
    xor %eax,%eax
.Lnested_dynamic_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_dynamic_zero
    mov $0x10000,%ebx
    xor %r8d,%r8d
.Lnested_dynamic_context:
    lea 0x1007(%rbx),%rax
    mov %rax,(%rbx)
    lea 0x2007(%rbx),%rax
    mov %rax,0x1000(%rbx)
    lea 0x3007(%rbx),%rax
    mov %rax,0x2000(%rbx)
    lea 0x3000(%rbx),%rdi
    mov $1,%ecx
.Lnested_dynamic_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax
    mov %rax,(%rdi,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_dynamic_identity
    movq $0x6035,48(%rdi)
    mov $0x8033,%eax
    test $1,%r8d
    jz .Lnested_dynamic_alias_ready
    mov $0x9033,%eax
.Lnested_dynamic_alias_ready:
    mov %rax,120(%rdi)
    add $0x4000,%rbx
    inc %r8d
    cmp $8,%r8d
    jne .Lnested_dynamic_context
    movq $0x1001e,0xc830
    movq $0,0xc838
