/* L1 constructs EPT12 with its own stores, in its private guest RAM.
 * The 64-leaf manager supports these two nonidentity aliases as well as
 * the boot identity mappings. No leaf outside the explicit set is valid. */
    mov $0x10000,%ebx
    movq $0x11007,(%rbx)
    mov $0x11000,%ebx
    movq $0x12007,(%rbx)
    mov $0x12000,%ebx
    movq $0x13007,(%rbx)
    mov $0x13000,%ebx
    xor %ecx,%ecx
.Lnested_map_clear_leaves:
    movq $0,(%rbx,%rcx,8)
    inc %ecx
    cmp $64,%ecx
    jne .Lnested_map_clear_leaves
    mov $1,%ecx
.Lnested_map_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax /* WB, RW, no execute */
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_map_identity
    movq $0x6035,48(%rbx) /* code GPA6000 RX */
    movq $0x8033,160(%rbx) /* GPA14000 -> L1 GPA8000 */
    movq $0x9033,168(%rbx) /* GPA15000 -> L1 GPA9000 */
    mov $0xc830,%ebx
    movq $0x1001e,(%rbx) /* four-level EPT, WB */
    movq $0,8(%rbx)
