/* Code and paging leaves remain WB/IPAT; only the two data aliases select
 * WT/IPAT for the explicit WT diagnostic, otherwise the original UC/IPAT. */
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
    xor %eax,%eax
.Lnested_uc_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_uc_zero
    movq $0x11007,0x10000
    movq $0x12007,0x11000
    movq $0x13007,0x12000
    mov $0x13000,%ebx
    mov $1,%ecx
.Lnested_uc_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x73,%rax
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_uc_identity
    movq $0x6075,48(%rbx)
#ifdef VMX_NESTED_WT_TEST
    movq $0xf063,0x13078 /* GPAf000 -> dedicated L1f000 WT/IPAT/RW */
    movq $0xf063,0x13070 /* GPAe000 -> same WT backing */
#else
    movq $0xf043,0x13078 /* GPAf000 -> dedicated L1f000 UC/IPAT/RW */
    movq $0xf043,0x13070 /* GPAe000 -> same dedicated L1f000 */
#endif
    movq $0x1001e,0xc830
    movq $0,0xc838
