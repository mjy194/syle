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
    cmp $8,%rdx
    je .Lnested_done
    ja .Lnested_fail
    lea .Latomic_catalog_keys(%rip),%rax
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
.Latomic_catalog_keys:
    .byte 0,0,1,1,0,1,2,0
