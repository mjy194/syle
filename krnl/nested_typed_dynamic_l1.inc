    /* Each owner runs WB A/B/C/A, UC C/B/A, restored WB B/A. L0 owns
     * the two stopped-domain type changes; L1 only selects immutable keys. */
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
    cmp $9,%rdx
    je .Lnested_done
    ja .Lnested_fail
    lea .Ltyped_dynamic_keys(%rip),%rax
    movzbl (%rax,%rdx),%eax
    shl $14,%rax
    add $0x1001e,%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.Ltyped_dynamic_keys:
    .byte 0,1,2,0,2,1,0,1,0
