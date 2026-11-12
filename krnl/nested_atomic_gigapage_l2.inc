/* N45: actual target reads/stores, with no target writes in phases 3/4.
 * A/B share backing but each owner/key/epoch has its own eight-byte slot. */
.global nested_atomic_multiround_call0,nested_atomic_multiround_call1
.global nested_atomic_multiround_call2,nested_atomic_multiround_call3
.global nested_atomic_multiround_call4,nested_atomic_multiround_call5
.global nested_atomic_multiround_call6,nested_atomic_multiround_call7
.global nested_atomic_multiround_call8,nested_atomic_multiround_call9,nested_atomic_multiround_call10,nested_atomic_multiround_call11,nested_atomic_multiround_call12
.global nested_parallel_call
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
    lea 0x42b00000(%rcx),%rax
    mov %rax,8(%r12)
    movq $0,16(%r12)
    movq $0,24(%r12)
    movq $0,32(%r12)
    movq $0,40(%r12)
    movq $0,48(%r12)
    movq $0,56(%r12)
    xor %r10d,%r10d
    jmp .Latomic_multiround_step
nested_l2_resume:
    mov 0xc820,%r10
    cmp $1,%r10
    jb .Lnested_fail
    cmp $12,%r10
    ja .Lnested_fail
.Latomic_multiround_step:
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    mov %rcx,%r12
    shl $6,%r12
    add $0xa000,%r12
    lea .Latomic_multiround_keys(%rip),%rax
    imul $13,%rcx,%rdx
    add %r10,%rdx
    movzbl (%rax,%rdx),%r8d /* selected key */
    movabs $0x3344334433443344,%rax
    cmp $2,%r8
    je .Latomic_multiround_c_read
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,24(%r12)
    jmp .Latomic_multiround_old_check
.Latomic_multiround_c_read:
    movabs $0x99aa99aa99aa99aa,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,48(%r12)
.Latomic_multiround_old_check:
    cmp $3,%r10
    jb .Latomic_multiround_access
    cmp $2,%r8
    je .Latomic_multiround_old_c
    /* Every A/B read after maintenance checks both owners' retained A/B.
     * This includes CPU1 A's data after its private root was evicted. */
    movabs $0x42b0000000000000,%rax
    cmp %rax,0xf040
    jne .Lnested_fail
    inc %rax
    cmp %rax,0xf048
    jne .Lnested_fail
    movabs $0x42b0000000000100,%rax
    cmp %rax,0xf080
    jne .Lnested_fail
    inc %rax
    cmp %rax,0xf088
    jne .Lnested_fail
    cmp $8,%r10
    jb .Latomic_multiround_access
    movabs $0x42b1000000000000,%rax
    cmp %rax,0xf140
    jne .Lnested_fail
    inc %rax
    cmp %rax,0xf148
    jne .Lnested_fail
    movabs $0x42b1000000000101,%rax
    cmp %rax,0xf188
    jne .Lnested_fail
    jmp .Latomic_multiround_access
.Latomic_multiround_old_c:
    movabs $0x42b0000000000102,%rax
    cmp %rax,0xf090
    jne .Lnested_fail
    cmp $8,%r10
    jb .Latomic_multiround_access
    movabs $0x42b1000000000102,%rax
    cmp %rax,0xf190
    jne .Lnested_fail
.Latomic_multiround_access:
    cmp $2,%r10
    jb .Latomic_multiround_old_write
    jne .Latomic_multiround_new_access
    test %rcx,%rcx
    jnz .Latomic_multiround_old_write
    jmp .Latomic_multiround_checkpoint
.Latomic_multiround_new_access:
    cmp $8,%r10
    jae .Latomic_multiround_second_access
    cmp $5,%r10
    jb .Latomic_multiround_checkpoint
    cmp $7,%r10
    je .Latomic_multiround_new_write
    cmp $5,%r10
    jne .Latomic_multiround_phase6
    test %rcx,%rcx
    jz .Latomic_multiround_new_write
    jmp .Latomic_multiround_checkpoint
.Latomic_multiround_phase6:
    test %rcx,%rcx
    jz .Latomic_multiround_checkpoint
.Latomic_multiround_new_write:
    movabs $0x42b1000000000000,%rax
    mov $0x140,%r9d
    jmp .Latomic_multiround_write
.Latomic_multiround_second_access:
    cmp $10,%r10
    jb .Latomic_multiround_checkpoint
    cmp $12,%r10
    je .Latomic_multiround_second_write
    cmp $10,%r10
    jne .Latomic_multiround_phase11
    test %rcx,%rcx
    jz .Latomic_multiround_second_write
    jmp .Latomic_multiround_checkpoint
.Latomic_multiround_phase11:
    test %rcx,%rcx
    jz .Latomic_multiround_checkpoint
.Latomic_multiround_second_write:
    movabs $0x42b2000000000000,%rax
    mov $0x240,%r9d
    jmp .Latomic_multiround_write
.Latomic_multiround_old_write:
    movabs $0x42b0000000000000,%rax
    mov $0x40,%r9d
.Latomic_multiround_write:
    mov %rcx,%rdx
    shl $8,%rdx
    add %rdx,%rax
    add %r8,%rax
    mov %rcx,%rdx
    shl $6,%rdx
    add %r9,%rdx
    lea (%rdx,%r8,8),%rdx
    mov %rax,0xf000(%rdx)
    cmp %rax,0xf000(%rdx)
    jne .Lnested_fail
    mov %rax,32(%r12)
    cmp $2,%r8
    jne .Latomic_multiround_checkpoint
    mov %rax,56(%r12)
.Latomic_multiround_checkpoint:
    /* Independent 4KiB offset of the same source large leaf. Reading must
     * remain private-RO even after a peer sets the shared PDE D bit. */
    mov 0x31000,%rdx
    mov 0x30000,%rdx
    cmp $8,%r10
    jb .Ln45_second_write_check
    movabs $0x4500000000000000,%rdx
    cmp %rdx,0x30000
    jne .Lnested_fail
    inc %rdx
    cmp %rdx,0x30008
    jne .Lnested_fail
    cmp %rdx,0x31008
    jne .Lnested_fail
    dec %rdx
    cmp %rdx,0x31000
    jne .Lnested_fail
.Ln45_second_write_check:
    cmp $7,%r10
    je .Ln45_second_write
    cmp $12,%r10
    jne .Ln45_second_done
.Ln45_second_write:
    movabs $0x4500000000000000,%rdx
    add %rcx,%rdx
    mov %rdx,0x31000(,%rcx,8)
    cmp %rdx,0x31000(,%rcx,8)
    jne .Lnested_fail
    mov %rdx,0x30000(,%rcx,8)
    cmp %rdx,0x30000(,%rcx,8)
    jne .Lnested_fail
.Ln45_second_done:
    lea 1(%r10),%rax
    mov %rax,40(%r12)
    cmp $12,%r10
    jne .Latomic_multiround_dispatch
    lea 0x3200(%rcx),%rax
    mov %rax,16(%r12)
.Latomic_multiround_dispatch:
    cmp $0,%r10
    je .Latomic_multiround_call0
    cmp $1,%r10
    je .Latomic_multiround_call1
    cmp $2,%r10
    je .Latomic_multiround_call2
    cmp $3,%r10
    je .Latomic_multiround_call3
    cmp $4,%r10
    je .Latomic_multiround_call4
    cmp $5,%r10
    je .Latomic_multiround_call5
    cmp $6,%r10
    je .Latomic_multiround_call6
    cmp $7,%r10
    je .Latomic_multiround_call7
    cmp $8,%r10
    je .Latomic_multiround_call8
    cmp $9,%r10
    je .Latomic_multiround_call9
    cmp $10,%r10
    je .Latomic_multiround_call10
    cmp $11,%r10
    je .Latomic_multiround_call11
    jmp .Latomic_multiround_call12
.Latomic_multiround_call0:
    pushq $2
    popfq
nested_parallel_call:
nested_atomic_multiround_call0:
    vmcall
    ud2
.Latomic_multiround_call1:
    pushq $2
    popfq
nested_atomic_multiround_call1:
    vmcall
    ud2
.Latomic_multiround_call2:
    pushq $2
    popfq
nested_atomic_multiround_call2:
    vmcall
    ud2
.Latomic_multiround_call3:
    pushq $2
    popfq
nested_atomic_multiround_call3:
    vmcall
    ud2
.Latomic_multiround_call4:
    pushq $2
    popfq
nested_atomic_multiround_call4:
    vmcall
    ud2
.Latomic_multiround_call5:
    pushq $2
    popfq
nested_atomic_multiround_call5:
    vmcall
    ud2
.Latomic_multiround_call6:
    pushq $2
    popfq
nested_atomic_multiround_call6:
    vmcall
    ud2
.Latomic_multiround_call7:
    pushq $2
    popfq
nested_atomic_multiround_call7:
    vmcall
    ud2
.Latomic_multiround_call8:
    pushq $2
    popfq
nested_atomic_multiround_call8:
    vmcall
    ud2
.Latomic_multiround_call9:
    pushq $2
    popfq
nested_atomic_multiround_call9:
    vmcall
    ud2
.Latomic_multiround_call10:
    pushq $2
    popfq
nested_atomic_multiround_call10:
    vmcall
    ud2
.Latomic_multiround_call11:
    pushq $2
    popfq
nested_atomic_multiround_call11:
    vmcall
    ud2
.Latomic_multiround_call12:
    pushq $2
    popfq
nested_atomic_multiround_call12:
    vmcall
    ud2
