/* Bounded twelve-read diagnostic for an explicitly six-entry context cache.
 * Native L2 reads, actual L1 VMWRITE/INVEPT; no RIP-dependent emulation. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $11,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    lea .Lnested_dynamic_expected(%rip),%rdi
    mov (%rdi,%rsi,8),%rdx
    cmp %rdx,%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_dynamic_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    incq 0xc820
    cmp $11,%rsi
    je .Lnested_done
    cmp $7,%rsi
    jne .Lnested_dynamic_no_edit
    /* A was evicted, H retained: change both without invalidation. */
    movq $0xa033,0x13078
    movq $0xb033,0x2f078
.Lnested_dynamic_no_edit:
    cmp $9,%rsi
    jne .Lnested_dynamic_no_single
    movq $0x2c01e,0xc830
    movq $0,0xc838
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_dynamic_no_single:
    cmp $10,%rsi
    jne .Lnested_dynamic_no_global
    movq $0xb033,0x13078
    movq $0xdead,0xc830 /* ignored EPTP for all-context invalidation */
    movq $0,0xc838
    mov $0xc830,%ebx
    mov $2,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_dynamic_no_global:
    lea .Lnested_dynamic_next_eptp(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.p2align 3
.Lnested_dynamic_expected:
    .quad 0x11,0x22,0x11,0x22,0x11,0x22,0x11,0x22,0x33,0x22,0x44,0x44
.Lnested_dynamic_next_eptp:
    .quad 0x1401e,0x1801e,0x1c01e,0x2001e,0x2401e,0x2801e,0x2c01e
    .quad 0x1001e,0x2c01e,0x2c01e,0x1001e
