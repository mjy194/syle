    nop /* distinct entry and resume labels for the existing fixture profile */
.global nested_l2_resume,nested_uc_context_call
.global nested_uc_warm_call,nested_uc_done_call
nested_l2_resume:
    mov $0xa800,%ebx
    mov (%rbx),%rdx
    add $0x77,%rdx
    mov $0x30000,%edi
    mov $0x31000,%esi
    mov %rdx,(%rdi)
    mov (%rsi),%rax
    cmp %rdx,%rax
    jne .Lnested_uc_context_bad
nested_uc_context_call:
nested_uc_warm_call:
nested_uc_done_call:
    vmcall
    jmp nested_l2_resume
.Lnested_uc_context_bad:
    ud2
