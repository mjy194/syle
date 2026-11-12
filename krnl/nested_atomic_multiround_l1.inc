    /* L0 prebuilds immutable WB/IPAT1/AD1 roots. L1 only selects keys. */
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%eax
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%eax
    jne .Lnested_fail
    incq 0xc820
    mov 0xc820,%rdx
    cmp $13,%rdx
    je .Lnested_done
    ja .Lnested_fail
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    imul $13,%rcx,%rcx
    add %rcx,%rdx
    lea .Latomic_multiround_keys(%rip),%rax
    movzbl (%rax,%rdx),%eax
    shl $14,%rax
    add $0x1005e,%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.Latomic_multiround_keys:
    .byte 0,1,0,0,1,0,1,1,0,1,0,1,1 /* CPU0 */
    .byte 0,1,2,2,1,1,2,1,2,1,1,2,1 /* CPU1 */
