.global nested_sparse_warm_call,nested_sparse_write_fault,nested_sparse_after_write
.global nested_sparse_read_fault,nested_sparse_done_call,nested_l2_resume
    mov $0x30000,%edi
    movq $0x11,(%rdi)
    mov $0x31000,%edi
    movq $0x22,(%rdi)
    mov $0x32000,%edi
    movq $0x33,(%rdi)
    mov $0x33000,%edi
    movq $0x44,(%rdi)
nested_sparse_warm_call:
    vmcall
nested_l2_resume:
    /* Refill three distinct EPT branches after the revocation epoch. */
    mov $0x30000,%edi
    cmpq $0x11,(%rdi)
    jne .Lnested_sparse_bad
    mov $0x31000,%edi
    cmpq $0x22,(%rdi)
    jne .Lnested_sparse_bad
    mov $0x32000,%edi
    cmpq $0x33,(%rdi)
    jne .Lnested_sparse_bad
    mov $0xaa,%eax
nested_sparse_write_fault:
    mov %rax,(%rdi)
nested_sparse_after_write:
    mov $0x33000,%esi
nested_sparse_read_fault:
    mov (%rsi),%rax
    cmp $0x11,%rax
    jne .Lnested_sparse_bad
nested_sparse_done_call:
    vmcall
.Lnested_sparse_bad:
    ud2
