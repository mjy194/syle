    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%eax
    jne .Lnested_fail
    incq 0xc820
    mov 0xc820,%rdx
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    cmp $6,%rdx
#else
    cmp $5,%rdx
#endif
    je .Lnested_done
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    cmp $5,%rdx
#else
    cmp $4,%rdx
#endif
    ja .Lnested_fail
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    mov $0x1001e,%eax
    cmp $1,%rdx
    jne .Loc_not_b
    mov $0x1401e,%eax
.Loc_not_b:
    cmp $2,%rdx
    je .Loc_key_c
    cmp $5,%rdx
    jne .Loc_key_ready
.Loc_key_c:
    mov $0x1801e,%eax
    jmp .Loc_key_ready
#else
    mov $0x1001e,%eax
    test $1,%dl
    jz .Loc_key_ready
    mov $0x1401e,%eax
#endif
.Loc_key_ready:
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    cmp $4,%rdx
    je .Loc_type1
    cmp $5,%rdx
#else
    cmp $2,%rdx
    je .Loc_type1
    cmp $3,%rdx
#endif
    jne .Loc_resume
    movabs $0xffffffffffffffff,%rax
    mov %rax,0xc830 /* type2 must ignore this deliberately invalid EPTP */
    mov $2,%eax
    jmp .Loc_invalidate
.Loc_type1:
    movq $0x1001e,0xc830
    mov $1,%eax
.Loc_invalidate:
    movq $0,0xc838
    mov $0xc830,%ebx
    invept (%rbx),%rax
    jbe .Lnested_fail
.Loc_resume:
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
