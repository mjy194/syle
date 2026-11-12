    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $3,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    test $1,%sil
    jnz .Lnested_ad_fail_call
    cmp $48,%rax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0xf000,%rax
    jne .Lnested_fail
    mov $0x6400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    and $0x1ff,%eax
    mov $0x181,%edx
    test %rsi,%rsi
    jz .Lnested_ad_fail_qual_ready
    mov $0x18a,%edx
.Lnested_ad_fail_qual_ready:
    cmp %edx,%eax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_ad_fail_read(%rip),%rdx
    test %rsi,%rsi
    jz .Lnested_ad_fail_rip_ready
    lea nested_ad_fail_write(%rip),%rdx
.Lnested_ad_fail_rip_ready:
    cmp %rdx,%rax
    jne .Lnested_fail
    /* Every traversed present ancestor acquires A, never nonleaf D. */
    mov 0x10000,%rax
    and $0x300,%eax
    cmp $0x100,%eax
    jne .Lnested_fail
    mov 0x11000,%rax
    and $0x300,%eax
    cmp $0x100,%eax
    jne .Lnested_fail
    mov 0x12000,%rax
    and $0x300,%eax
    cmp $0x100,%eax
    jne .Lnested_fail
    test %rsi,%rsi
    jnz .Lnested_ad_fail_grant_write
    cmpq $0xdead000,0x13078
    jne .Lnested_fail
    movq $0x8031,0x13078
    jmp .Lnested_ad_fail_invalidate
.Lnested_ad_fail_grant_write:
    cmpq $0x8131,0x13078 /* A set, D clear, still read-only */
    jne .Lnested_fail
    movq $0x8033,0x13078 /* L1 grants W and deliberately resets A/D */
.Lnested_ad_fail_invalidate:
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
    incq 0xc820
    mov $0xf000,%edi /* preserve the original L2 memory operand */
    vmresume /* original fault RIP; do not skip either access */
    jmp .Lnested_fail
.Lnested_ad_fail_call:
    cmp $18,%rax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%r9
    jbe .Lnested_fail
    cmp $3,%rsi
    je .Lnested_ad_fail_final
    lea nested_ad_fail_read_done(%rip),%rdx
    cmp %rdx,%r9
    jne .Lnested_fail
    cmp $0x11,%r8
    jne .Lnested_fail
    cmpq $0x8131,0x13078
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    lea 3(%r9),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    incq 0xc820
    vmresume
    jmp .Lnested_fail
.Lnested_ad_fail_final:
    lea nested_ad_fail_done(%rip),%rdx
    cmp %rdx,%r9
    jne .Lnested_fail
    incq 0xc820
    jmp .Lnested_done
