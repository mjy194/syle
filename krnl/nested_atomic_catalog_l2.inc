.global nested_atomic_catalog_call0,nested_atomic_catalog_call1
.global nested_atomic_catalog_call2,nested_atomic_catalog_call3
.global nested_atomic_catalog_call4,nested_atomic_catalog_call5
.global nested_atomic_catalog_call6,nested_atomic_catalog_call7
.global nested_parallel_call
    /* Private counters and owner mailbox; no guest EPT12 structural stores.
     * L0 checkpoint barriers coordinate all eight stages, not retirement overlap. */
    cmpq $0,0xc820
    jne .Lnested_fail
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    mov %rcx,%r12
    shl $6,%r12
    add $0xa000,%r12
    lea 0x3200(%rcx),%rax
    mov %rax,(%r12)
    lea 0x41c00000(%rcx),%rax
    mov %rax,8(%r12)
    movq $0,16(%r12)
    movq $0,24(%r12)
    movq $0,32(%r12)
    movq $0,40(%r12)
    movq $0,48(%r12)
    movq $0,56(%r12)
    xor %r10d,%r10d
    jmp .Latomic_catalog_step
nested_l2_resume:
    mov 0xc820,%r10
    cmp $1,%r10
    jb .Lnested_fail
    cmp $7,%r10
    ja .Lnested_fail
.Latomic_catalog_step:
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    mov %rcx,%r12
    shl $6,%r12
    add $0xa000,%r12
    cmp $6,%r10
    je .Latomic_catalog_c
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,24(%r12)
    test %r10,%r10
    jz .Latomic_catalog_call0
    cmp $1,%r10
    je .Latomic_catalog_a_first
    cmp $2,%r10
    je .Latomic_catalog_call2
    cmp $3,%r10
    je .Latomic_catalog_b_write
    cmp $4,%r10
    je .Latomic_catalog_a_peer
    /* Warm B and recreated A observe the same original backing page. */
    movabs $0x41c0000000000000,%rax
    cmp %rax,0xf008
    jne .Lnested_fail
    movabs $0x41cb000000000000,%rax
    cmp %rax,0xf010
    jne .Lnested_fail
    inc %rax
    cmp %rax,0xf018
    jne .Lnested_fail
    mov 0xf010(,%rcx,8),%rax
    mov %rax,32(%r12)
    movabs $0x41ca000000000001,%rax
    cmp %rax,0xf020
    jne .Lnested_fail
    mov %rax,40(%r12)
    cmp $5,%r10
    je .Latomic_catalog_call5
    lea 0x3200(%rcx),%rax
    mov %rax,16(%r12)
    jmp .Latomic_catalog_call7
.Latomic_catalog_a_first:
    test %rcx,%rcx
    jnz .Latomic_catalog_call1
    movabs $0x41c0000000000000,%rax
    mov %rax,0xf008
    cmp %rax,0xf008
    jne .Lnested_fail
    jmp .Latomic_catalog_call1
.Latomic_catalog_b_write:
    movabs $0x41cb000000000000,%rax
    add %rcx,%rax
    mov %rax,0xf010(,%rcx,8)
    cmp %rax,0xf010(,%rcx,8)
    jne .Lnested_fail
    mov %rax,32(%r12)
    jmp .Latomic_catalog_call3
.Latomic_catalog_a_peer:
    test %rcx,%rcx
    jz .Latomic_catalog_call4
    movabs $0x41ca000000000001,%rax
    mov %rax,0xf020
    cmp %rax,0xf020
    jne .Lnested_fail
    jmp .Latomic_catalog_call4
.Latomic_catalog_c:
    movabs $0x99aa99aa99aa99aa,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,48(%r12)
    movabs $0x41cc000000000000,%rax
    add %rcx,%rax
    mov %rax,0xf008(,%rcx,8)
    cmp %rax,0xf008(,%rcx,8)
    jne .Lnested_fail
    mov %rax,56(%r12)
    jmp .Latomic_catalog_call6
.Latomic_catalog_call0:
    pushq $2
    popfq
nested_parallel_call:
nested_atomic_catalog_call0:
    vmcall
    ud2
.Latomic_catalog_call1:
    pushq $2
    popfq
nested_atomic_catalog_call1:
    vmcall
    ud2
.Latomic_catalog_call2:
    pushq $2
    popfq
nested_atomic_catalog_call2:
    vmcall
    ud2
.Latomic_catalog_call3:
    pushq $2
    popfq
nested_atomic_catalog_call3:
    vmcall
    ud2
.Latomic_catalog_call4:
    pushq $2
    popfq
nested_atomic_catalog_call4:
    vmcall
    ud2
.Latomic_catalog_call5:
    pushq $2
    popfq
nested_atomic_catalog_call5:
    vmcall
    ud2
.Latomic_catalog_call6:
    pushq $2
    popfq
nested_atomic_catalog_call6:
    vmcall
    ud2
.Latomic_catalog_call7:
    pushq $2
    popfq
nested_atomic_catalog_call7:
    vmcall
    ud2
