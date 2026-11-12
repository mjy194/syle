/* Code/paging remain WB/IPAT. UC data leaf does not ignore guest PAT. */
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
    xor %eax,%eax
.Lnested_uc_pat_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_uc_pat_zero
    movq $0x11007,0x10000
    movq $0x12007,0x11000
    movq $0x13007,0x12000
    mov $0x13000,%ebx
    mov $1,%ecx
.Lnested_uc_pat_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x73,%rax
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_uc_pat_identity
    movq $0x6075,48(%rbx)
    movq $0xf003,0x13078 /* GPAf000 -> dedicated L1f000 UC, IPAT=0 */
    movq $0x1001e,0xc830
    movq $0,0xc838
