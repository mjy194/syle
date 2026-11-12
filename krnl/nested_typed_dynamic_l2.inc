.global nested_typed_dynamic_call0,nested_typed_dynamic_call1,nested_typed_dynamic_call2
.global nested_typed_dynamic_call3,nested_typed_dynamic_call4,nested_typed_dynamic_call5
.global nested_typed_dynamic_call6,nested_typed_dynamic_call7,nested_typed_dynamic_call8
.global nested_typed_warm_call,nested_typed_uc_entry,nested_typed_uc_call
.global nested_typed_wb_entry,nested_typed_wb_call,nested_parallel_call
.global nested_typed_context_call0,nested_typed_context_call1,nested_typed_context_call2
.global nested_typed_context_call3,nested_typed_context_call4,nested_typed_context_call5
    /* No guest peer-heartbeat wait. B2 coordinates only at L0 safe points.
     * Every store stays in this owner's trace or its own target words. */
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
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    lea 0x39b20000(%rcx),%rax
#else
    lea 0x38cd0000(%rcx),%rax
#endif
    mov %rax,8(%r12) /* profile marker only; causal overlap is proved in L0 */
    movq $0,16(%r12)
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,24(%r12)
    movq $0,32(%r12)
    movq $0,40(%r12)
    movq $0,48(%r12)
    movq $0,56(%r12)
    xor %r10d,%r10d
    jmp .Ltyped_dynamic_step
nested_l2_resume:
nested_typed_uc_entry:
nested_typed_wb_entry:
    mov 0xc820,%r10
    cmp $1,%r10
    jb .Lnested_fail
    cmp $8,%r10
    ja .Lnested_fail
.Ltyped_dynamic_step:
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    mov %rcx,%r12
    shl $6,%r12
    add $0xa000,%r12
    lea .Ltyped_dynamic_keys(%rip),%rdx
    movzbl (%rdx,%r10),%r8d
    movabs $0x0001000100010001,%rdx
    imul %r8,%rdx
    movabs $0x38ca38ca38ca38ca,%rax
    add %rdx,%rax
    cmp %rax,0x30000
    jne .Lnested_fail
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    cmp $4,%r10
    jb .Ltyped_dynamic_call
    imul $3,%rcx,%rdx
    add %r8,%rdx
    movabs $0x38cd38cd38cd3800,%rax
    add %rdx,%rax
    cmp $7,%r10
    jae .Ltyped_dynamic_restored
    mov %rax,0xf008(,%rdx,8)
    cmp %rax,0xf008(,%rdx,8)
    jne .Lnested_fail
    mov %rax,32(%r12,%r8,8)
    jmp .Ltyped_dynamic_call
.Ltyped_dynamic_restored:
    cmp %rax,0xf008(,%rdx,8)
    jne .Lnested_fail
    cmp $8,%r10
    jne .Ltyped_dynamic_call
    mov %rax,56(%r12)
    lea 0x3200(%rcx),%rdx
    mov %rdx,16(%r12)
.Ltyped_dynamic_call:
    cmp $0,%r10
    je .Ltyped_dynamic_call0
    cmp $1,%r10
    je .Ltyped_dynamic_call1
    cmp $2,%r10
    je .Ltyped_dynamic_call2
    cmp $3,%r10
    je .Ltyped_dynamic_call3
    cmp $4,%r10
    je .Ltyped_dynamic_call4
    cmp $5,%r10
    je .Ltyped_dynamic_call5
    cmp $6,%r10
    je .Ltyped_dynamic_call6
    cmp $7,%r10
    je .Ltyped_dynamic_call7
    pushq $2
    popfq
nested_typed_dynamic_call8:
    vmcall
    ud2
.Ltyped_dynamic_call0:
    pushq $2
    popfq
nested_typed_warm_call:
nested_parallel_call:
nested_typed_context_call0:
nested_typed_dynamic_call0:
    vmcall
    ud2
.Ltyped_dynamic_call1:
    pushq $2
    popfq
nested_typed_context_call1:
nested_typed_dynamic_call1:
    vmcall
    ud2
.Ltyped_dynamic_call2:
    pushq $2
    popfq
nested_typed_context_call2:
nested_typed_dynamic_call2:
    vmcall
    ud2
.Ltyped_dynamic_call3:
    pushq $2
    popfq
nested_typed_context_call3:
nested_typed_dynamic_call3:
    vmcall
    ud2
.Ltyped_dynamic_call4:
    pushq $2
    popfq
nested_typed_uc_call:
nested_typed_context_call4:
nested_typed_dynamic_call4:
    vmcall
    ud2
.Ltyped_dynamic_call5:
    pushq $2
    popfq
nested_typed_context_call5:
nested_typed_dynamic_call5:
    vmcall
    ud2
.Ltyped_dynamic_call6:
    pushq $2
    popfq
nested_typed_dynamic_call6:
    vmcall
    ud2
.Ltyped_dynamic_call7:
    pushq $2
    popfq
nested_typed_wb_call:
nested_typed_dynamic_call7:
    vmcall
    ud2
