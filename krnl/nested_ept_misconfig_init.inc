    movq $0x42,0x8000 /* deterministic data for all repaired aliases */
/* L1 owns four table pages. Large leaves alias offsets into base-zero
 * private backing; the adapter independently validates accessible backing. */
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
    xor %eax,%eax
.Lnested_misconfig_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_misconfig_zero
    movq $0x11007,0x10000
    movq $0x12007,0x11000
    movq $0x13007,0x12000
    mov $0x13000,%ebx
    mov $1,%ecx
.Lnested_misconfig_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_misconfig_identity
    movq $0x6035,48(%rbx)
    movq $0x10b3,0x12008 /* malformed 2-MiB leaf: base not 2-MiB aligned */
    movq $0x8032,0x13078 /* malformed f000 leaf: W without R */
    movq $0x8013,0x13070 /* malformed e000 leaf: reserved memory type2 */
    movq $0x1001e,0xc830
    movq $0,0xc838
