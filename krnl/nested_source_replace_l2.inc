/* N47B: one shared A/B source leaf changes backing after phase 2.
 * Every phase ends in a real L2 VMCALL, reflected to L1 before L0's barrier.
 * Only data slots are written; L1/L2 never write an EPT12 source table.
 * CPU0: old A write, old B write, old A read, new A read, new B read,
 *       new A write, new A read, new B read.
 * CPU1: old A write, old B write, C write, C read, new B read,
 *       new B read, new A write, C read. */
.global nested_source_replace_call0,nested_source_replace_call1
.global nested_source_replace_call2,nested_source_replace_call3
.global nested_source_replace_call4,nested_source_replace_call5
.global nested_source_replace_call6,nested_source_replace_call7
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
    lea 0x47b00000(%rcx),%rax
    mov %rax,8(%r12)
    movq $0,16(%r12)
    movq $0,24(%r12)
    movq $0,32(%r12)
    movq $0,40(%r12)
    movq $0,48(%r12)
    movq $0,56(%r12)
    xor %r10d,%r10d
    jmp .Lsource_replace_step
nested_l2_resume:
    mov 0xc820,%r10
    cmp $1,%r10
    jb .Lnested_fail
    cmp $7,%r10
    ja .Lnested_fail
.Lsource_replace_step:
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    mov %rcx,%r12
    shl $6,%r12
    add $0xa000,%r12
    lea .Lsource_replace_keys(%rip),%rax
    lea (%r10,%rcx,8),%rdx
    movzbl (%rax,%rdx),%r8d
    cmp $2,%r8
    je .Lsource_replace_c_read
    cmp $3,%r10
    jae .Lsource_replace_new_read
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,24(%r12)
    cmp $2,%r10
    jb .Lsource_replace_access
    /* The phase-1 barrier made both owners' old A/B stores visible. */
    movabs $0x47b0000000000000,%rax
    cmp %rax,0xf040
    jne .Lnested_fail
    inc %rax
    cmp %rax,0xf048
    jne .Lnested_fail
    movabs $0x47b0000000000100,%rax
    cmp %rax,0xf080
    jne .Lnested_fail
    inc %rax
    cmp %rax,0xf088
    jne .Lnested_fail
    jmp .Lsource_replace_access
.Lsource_replace_c_read:
    movabs $0x99aa99aa99aa99aa,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,48(%r12)
    cmp $3,%r10
    jb .Lsource_replace_access
    movabs $0x47b0000000000102,%rax
    cmp %rax,0xf090
    jne .Lnested_fail
    jmp .Lsource_replace_access
.Lsource_replace_new_read:
    movabs $0x5566556655665566,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,24(%r12)
    /* A fresh replacement page cannot contain the retained old-page slots. */
    cmpq $0,0xf040
    jne .Lnested_fail
    cmpq $0,0xf048
    jne .Lnested_fail
    cmpq $0,0xf080
    jne .Lnested_fail
    cmpq $0,0xf088
    jne .Lnested_fail
    cmp $5,%r10
    jae .Lsource_replace_late_read
    /* Phases 3/4 only refill: neither owner may dirty the new data. */
    cmpq $0,0xf140
    jne .Lnested_fail
    cmpq $0,0xf180
    jne .Lnested_fail
    jmp .Lsource_replace_access
.Lsource_replace_late_read:
    cmp $5,%r10
    je .Lsource_replace_phase5_read
    /* CPU0's phase-5 write is visible after that checkpoint on both owners. */
    movabs $0x47b1000000000000,%rax
    cmp %rax,0xf140
    jne .Lnested_fail
    cmp $7,%r10
    jne .Lsource_replace_access
    /* CPU0's final B read sees CPU1's independently trapped A write. */
    movabs $0x47b1000000000100,%rax
    cmp %rax,0xf180
    jne .Lnested_fail
    jmp .Lsource_replace_access
.Lsource_replace_phase5_read:
    /* CPU0 may already be writing its own slot while CPU1 reads B. */
    cmpq $0,0xf180
    jne .Lnested_fail
.Lsource_replace_access:
    cmp $2,%r10
    jb .Lsource_replace_old_write
    jne .Lsource_replace_new_access
    test %rcx,%rcx
    jnz .Lsource_replace_old_write
    jmp .Lsource_replace_checkpoint
.Lsource_replace_new_access:
    cmp $5,%r10
    je .Lsource_replace_phase5_write
    cmp $6,%r10
    jne .Lsource_replace_checkpoint
    test %rcx,%rcx
    jnz .Lsource_replace_new_write
    jmp .Lsource_replace_checkpoint
.Lsource_replace_phase5_write:
    test %rcx,%rcx
    jnz .Lsource_replace_checkpoint
.Lsource_replace_new_write:
    movabs $0x47b1000000000000,%rax
    mov $0x140,%r9d
    jmp .Lsource_replace_write
.Lsource_replace_old_write:
    movabs $0x47b0000000000000,%rax
    mov $0x40,%r9d
.Lsource_replace_write:
    mov %rcx,%rdx
    shl $8,%rdx
    add %rdx,%rax
    add %r8,%rax
    mov %rcx,%rdx
    shl $6,%rdx
    add %r9,%rdx
    lea (%rdx,%r8,8),%rdx
    /* Each owner/key/epoch uses its own slot, initially zero. */
    cmpq $0,0xf000(%rdx)
    jne .Lnested_fail
    mov %rax,0xf000(%rdx)
    cmp %rax,0xf000(%rdx)
    jne .Lnested_fail
    mov %rax,32(%r12)
    cmp $2,%r8
    jne .Lsource_replace_checkpoint
    mov %rax,56(%r12)
.Lsource_replace_checkpoint:
    lea 1(%r10),%rax
    mov %rax,40(%r12)
    cmp $7,%r10
    jne .Lsource_replace_dispatch
    lea 0x3200(%rcx),%rax
    mov %rax,16(%r12)
.Lsource_replace_dispatch:
    cmp $0,%r10
    je .Lsource_replace_call0
    cmp $1,%r10
    je .Lsource_replace_call1
    cmp $2,%r10
    je .Lsource_replace_call2
    cmp $3,%r10
    je .Lsource_replace_call3
    cmp $4,%r10
    je .Lsource_replace_call4
    cmp $5,%r10
    je .Lsource_replace_call5
    cmp $6,%r10
    je .Lsource_replace_call6
    jmp .Lsource_replace_call7
.Lsource_replace_call0:
    pushq $2
    popfq
nested_parallel_call:
nested_source_replace_call0:
    vmcall
    ud2
.Lsource_replace_call1:
    pushq $2
    popfq
nested_source_replace_call1:
    vmcall
    ud2
.Lsource_replace_call2:
    pushq $2
    popfq
nested_source_replace_call2:
    vmcall
    ud2
.Lsource_replace_call3:
    pushq $2
    popfq
nested_source_replace_call3:
    vmcall
    ud2
.Lsource_replace_call4:
    pushq $2
    popfq
nested_source_replace_call4:
    vmcall
    ud2
.Lsource_replace_call5:
    pushq $2
    popfq
nested_source_replace_call5:
    vmcall
    ud2
.Lsource_replace_call6:
    pushq $2
    popfq
nested_source_replace_call6:
    vmcall
    ud2
.Lsource_replace_call7:
    pushq $2
    popfq
nested_source_replace_call7:
    vmcall
    ud2
