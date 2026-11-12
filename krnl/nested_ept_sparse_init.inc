/* Ten private EPT12 table pages, built by actual L1 instructions. */
    mov $0x10000,%edi
    mov $(10*4096/8),%ecx
    xor %eax,%eax
.Lnested_sparse_zero:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_sparse_zero
    movq $0x11007,0x10000
    movq $0x17007,0x10008
    movq $0x12007,0x11000
    movq $0x15007,0x11008
    movq $0x13007,0x12000
    movq $0x14007,0x12008
    mov $0x13000,%ebx
    mov $1,%ecx
.Lnested_sparse_identity:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_sparse_identity
    movq $0x6035,48(%rbx) /* low code RX */
    movq $0xb033,120(%rbx) /* GPA f000 -> b000 */
    movq $0x8033,0x14008 /* GPA 201000 -> 8000 */
    movq $0x16007,0x15000
    movq $0x9033,0x16008 /* GPA 40001000 -> 9000 */
    movq $0x18007,0x17000
    movq $0x19007,0x18000
    movq $0xa033,0x19008 /* GPA 8000001000 -> a000 */
    movq $0x1001e,0xc830
    movq $0,0xc838
