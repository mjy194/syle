    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $5,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    lea .Lnested_ad_context_values(%rip),%rdi
    cmp (%rdi,%rsi,8),%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_ad_context_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    lea .Lnested_ad_context_a_leaves(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    cmp %rax,0x13078
    jne .Lnested_fail
    lea .Lnested_ad_context_b_leaves(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    cmp %rax,0x17078
    jne .Lnested_fail
    incq 0xc820
    cmp $5,%rsi
    je .Lnested_done
    cmp $1,%rsi
    je .Lnested_ad_context_clear_a
    cmp $3,%rsi
    jne .Lnested_ad_context_resume
    /* Clear both EPT12 A/D sets; all-context invalidation requires B also
     * to re-observe read-only activity rather than retain the old D bit. */
    mov $(8*4096/8),%ecx
    jmp .Lnested_ad_context_clear_start
.Lnested_ad_context_clear_a:
    mov $(4*4096/8),%ecx
.Lnested_ad_context_clear_start:
    mov $0x10000,%edi
.Lnested_ad_context_clear_loop:
    andq $-769,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_ad_context_clear_loop
    movq $0,0xc838
    cmp $1,%rsi
    jne .Lnested_ad_context_all
    movq $0x1005e,0xc830 /* invalidate A while B is current */
    mov $1,%eax
    jmp .Lnested_ad_context_invalidate
.Lnested_ad_context_all:
    movq $0xdead,0xc830 /* ignored low qword for type2 */
    mov $2,%eax
.Lnested_ad_context_invalidate:
    mov $0xc830,%ebx
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_ad_context_resume:
    lea .Lnested_ad_context_next(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    /* Restore the requested L2 operation last, after all VMX emulation. */
    xor %r10d,%r10d
    xor %r11d,%r11d
    cmp $0,%rsi
    je .Lnested_ad_context_write_b
    cmp $2,%rsi
    jne .Lnested_ad_context_enter
    mov $1,%r10d
    mov $0x88,%r11d
    jmp .Lnested_ad_context_enter
.Lnested_ad_context_write_b:
    mov $1,%r10d
    mov $0x77,%r11d
.Lnested_ad_context_enter:
    vmresume
    jmp .Lnested_fail
.p2align 3
.Lnested_ad_context_values:
    .quad 0x11,0x77,0x11,0x88,0x77,0x88
.Lnested_ad_context_a_leaves:
    .quad 0x8133,0x8133,0x8133,0x8333,0x8033,0x8133
.Lnested_ad_context_b_leaves:
    .quad 0x9033,0x9333,0x9333,0x9333,0x9133,0x9133
.Lnested_ad_context_next:
    .quad 0x1405e,0x1005e,0x1005e,0x1405e,0x1005e
