.global nested_ad_fail_read,nested_ad_fail_read_done,nested_ad_fail_write,nested_ad_fail_done
.global nested_l2_resume
    mov $0xf000,%edi
nested_ad_fail_read:
    mov (%rdi),%rax
    cmp $0x11,%rax
    jne .Lnested_ad_fail_bad
nested_ad_fail_read_done:
    vmcall
nested_l2_resume:
    mov $0xf000,%edi
nested_ad_fail_write:
    movq $0x77,(%rdi)
nested_ad_fail_done:
    vmcall
.Lnested_ad_fail_bad:
    ud2
