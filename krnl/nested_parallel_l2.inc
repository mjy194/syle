/* Both independently virtualized L2s must be simultaneously executing before
 * either may complete. No host writes these heartbeat words after publication.
 * Fixed trusted diagnostic RAM layout, not nested opcode dispatch policy. */
    mov 0xc828,%r11
    cmp $1,%r11
    ja .Lnested_fail
    mov %r11,%r12
    shl $6,%r12
    add $0xa000,%r12
    mov %r11,%r13
    xor $1,%r13
    shl $6,%r13
    add $0xa000,%r13
    add $0x3200,%r11
#ifdef VMX_NESTED_DOMAIN_TEST
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov 0xf000,%rax
    mov %rax,24(%r12)
#endif
    mov %r11,(%r12)
    mfence
    rdtsc
    shl $32,%rdx
    or %rdx,%rax
    mov %rax,%r10
.Lparallel_arrived:
    mov (%r13),%r9
    test %r9,%r9
    jnz .Lparallel_seen
    pause
    rdtsc
    shl $32,%rdx
    or %rdx,%rax
    sub %r10,%rax
    movabs $30000000000,%rdx
    cmp %rdx,%rax
    ja .Lnested_fail
    jmp .Lparallel_arrived
.Lparallel_seen:
    mov %r9,8(%r12)
    mfence
.Lparallel_observed:
    cmp %r11,8(%r13)
    je .Lparallel_complete
    pause
    rdtsc
    shl $32,%rdx
    or %rdx,%rax
    sub %r10,%rax
    movabs $30000000000,%rdx
    cmp %rdx,%rax
    ja .Lnested_fail
    jmp .Lparallel_observed
.Lparallel_complete:
#ifdef VMX_NESTED_OWNER_AD_TEST
    /* Both have read the same page. Only AP writes a disjoint data word;
     * BSP never reads that word until both are in root. */
    cmpq $1,0xc828
    jne .Lowner_ad_readonly
    movabs $0x35ad35ad35ad35ad,%rax
    mov %rax,0xf008
    mov %rax,32(%r12)
    jmp .Lowner_ad_finished
.Lowner_ad_readonly:
    mov 0xf010,%rax
    mov %rax,32(%r12)
.Lowner_ad_finished:
#endif
    mov %r11,16(%r12)
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    cmpq $1,0xc828
    jne .Ldomain_not_ap
 .global nested_domain_kick_begin
nested_domain_kick_begin:
    movq $1,0xa108
    mfence
.global nested_domain_kick_loop,nested_domain_kick_loop_end
nested_domain_kick_loop:
    pause
    cmpq $0,0xa100
    je nested_domain_kick_loop
nested_domain_kick_loop_end:
    movabs $0x5566556655665566,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,40(%r12)
.Ldomain_not_ap:
#endif
    pushq $2
    popfq
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
    mov 0xf000,%rax
#endif
.global nested_parallel_call
nested_parallel_call:
