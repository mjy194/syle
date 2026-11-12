.global nested_misconfig_fault0,nested_misconfig_fault1,nested_misconfig_fault2
.global nested_misconfig_done_call,nested_l2_resume
    mov $0xf000,%esi
nested_l2_resume:
nested_misconfig_fault0:
    mov (%rsi),%rax
    cmp $0x42,%rax
    jne .Lnested_misconfig_bad
    mov $0xe000,%esi
nested_misconfig_fault1:
    mov (%rsi),%rax
    cmp $0x42,%rax
    jne .Lnested_misconfig_bad
    mov $0x30000,%esi
nested_misconfig_fault2:
    mov (%rsi),%rax
    cmp $0x42,%rax
    jne .Lnested_misconfig_bad
nested_misconfig_done_call:
    vmcall
.Lnested_misconfig_bad:
    ud2
