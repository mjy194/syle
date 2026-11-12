.global nested_uc_warm_call,nested_uc_done_call,nested_l2_resume
    mov $0x30000,%edi /* PAT0 WB */
    mov $0x31000,%esi /* PAT1 UC- */
    mov $0x32000,%ebx /* PAT2 WT */
    movq $0x77,(%rdi)
    mov (%rdi),%rax
    cmp $0x77,%rax
    jne .Lnested_uc_pat_bad
    mov (%rsi),%rax
    cmp $0x77,%rax
    jne .Lnested_uc_pat_bad
    mov (%rbx),%rax
    cmp $0x77,%rax
    jne .Lnested_uc_pat_bad
nested_uc_warm_call:
    vmcall
nested_l2_resume:
    /* L1 scratch registers are not preserved across reflection. */
    mov $0x30000,%edi
    mov $0x31000,%esi
    mov $0x32000,%ebx
    movq $0x99,(%rsi)
    mov (%rdi),%rax
    cmp $0x99,%rax
    jne .Lnested_uc_pat_bad
    mov (%rsi),%rax
    cmp $0x99,%rax
    jne .Lnested_uc_pat_bad
    mov (%rbx),%rax
    cmp $0x99,%rax
    jne .Lnested_uc_pat_bad
nested_uc_done_call:
    vmcall
.Lnested_uc_pat_bad:
    ud2 /* the attempted unsafe third entry must never execute guest code */
