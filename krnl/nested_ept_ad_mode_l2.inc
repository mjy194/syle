    xor %r10d,%r10d /* initial A access is a read */
.global nested_l2_resume,nested_ad_mode_call
nested_l2_resume:
    mov $0xf000,%edi
    test %r10,%r10
    jz .Lnested_ad_mode_read
    mov %r11,(%rdi)
.Lnested_ad_mode_read:
    mov (%rdi),%rax
nested_ad_mode_call:
    vmcall
    ud2
