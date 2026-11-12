.global nested_ad_read_call,nested_ad_write_call,nested_ad_reread_call,nested_ad_rewrite_call
.global nested_l2_resume
    mov $0x30000,%edi
    mov (%rdi),%rax
    cmp $0x11,%rax
    jne .Lnested_ad_bad
nested_ad_read_call:
    vmcall
nested_l2_resume:
    mov $0x30000,%edi
    movq $0x77,(%rdi)
nested_ad_write_call:
    vmcall
    mov $0x30000,%edi
    mov (%rdi),%rax
    cmp $0x77,%rax
    jne .Lnested_ad_bad
nested_ad_reread_call:
    vmcall
    mov $0x30000,%edi
    movq $0x88,(%rdi)
nested_ad_rewrite_call:
    vmcall
.Lnested_ad_bad:
    ud2
