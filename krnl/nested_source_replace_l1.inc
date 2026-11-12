    /* N47B: L0 owns the source replacement after the phase-2 L1 return.
     * L1 only selects complete WB/IPAT1/AD1 EPTP keys. */
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
    cmp $8,%rdx
    je .Lnested_done
    ja .Lnested_fail
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    shl $3,%rcx
    add %rcx,%rdx
    lea .Lsource_replace_keys(%rip),%rax
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
.Lsource_replace_keys:
    .byte 0,1,0,0,1,0,0,1 /* CPU0: A/B/A/A/B/A/A/B */
    .byte 0,1,2,2,1,1,0,2 /* CPU1: A/B/C/C/B/B/A/C */
