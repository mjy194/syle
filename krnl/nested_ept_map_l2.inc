.global nested_map_warm_call,nested_map_write_fault,nested_map_after_write
.global nested_map_read_fault,nested_map_done_call,nested_l2_resume
    mov $0x14000,%edi
    mov $0x15000,%esi
    movq $0x77,(%rdi)
    movq $0x88,(%rsi)
nested_map_warm_call:
    vmcall
nested_l2_resume:
    mov $0x14000,%edi
    mov $0xaa,%eax
nested_map_write_fault:
    mov %rax,(%rdi)
nested_map_after_write:
    mov $0x15000,%esi
nested_map_read_fault:
    mov (%rsi),%rax
    cmp $0x99,%rax
    jne .Lnested_map_l2_bad
nested_map_done_call:
    vmcall
.Lnested_map_l2_bad:
    ud2
