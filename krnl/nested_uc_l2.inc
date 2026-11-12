.global nested_uc_warm_call,nested_uc_done_call,nested_l2_resume
    mov $0x30000,%edi /* guest PAT0 WB */
    mov $0x31000,%esi /* guest PAT1 UC; EPT ignores both via IPAT */
    movq $0x77,(%rdi)
    mov (%rsi),%rax
    cmp $0x77,%rax
    jne .Lnested_uc_bad
nested_uc_warm_call:
    vmcall
nested_l2_resume:
    mov $0x30000,%edi
    mov $0x31000,%esi
    movq $0x99,(%rsi)
    mov (%rdi),%rax
    cmp $0x99,%rax
    jne .Lnested_uc_bad
nested_uc_done_call:
    vmcall
.Lnested_uc_bad:
    ud2
