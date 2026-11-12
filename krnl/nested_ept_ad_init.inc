/* L1 enables EPT12 A/D through EPTP bit6, builds initially clear A/D. */
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
    xor %eax,%eax
.Lnested_ad_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_ad_zero
    movq $0x11007,0x10000
    movq $0x12007,0x11000
    movq $0x13007,0x12000
    mov $0x13000,%ebx
    mov $1,%ecx
.Lnested_ad_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_ad_identity
    movq $0x6035,48(%rbx)
    movq $0xb3,0x12008 /* 2-MiB data leaf, base0, RW/WB */
    movq $0x1005e,0xc830
    movq $0,0xc838
