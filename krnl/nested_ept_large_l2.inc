.global nested_large_warm_call,nested_large_write_fault,nested_large_after_write
.global nested_large_read_fault,nested_large_done_call,nested_l2_resume
    mov $0x30000,%edi
    movq $0x11,(%rdi)
    mov $0x31000,%edi
    movq $0x22,(%rdi)
nested_large_warm_call:
    vmcall
nested_l2_resume:
    mov $0x30000,%edi
    mov $0xaa,%eax
nested_large_write_fault:
    mov %rax,(%rdi)
nested_large_after_write:
    mov $0x31000,%esi
nested_large_read_fault:
    mov (%rsi),%rax
    cmp $0x22,%rax
    jne .Lnested_large_bad
nested_large_done_call:
    vmcall
.Lnested_large_bad:
    ud2
