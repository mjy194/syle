.global nested_memory_call,nested_l2_resume
    nop
nested_l2_resume:
    mov $0x30000,%edi
    mov $0x31000,%esi
    movq $0x77,(%rdi)
    cmpq $0x77,(%rsi)
    jne .Lnested_memory_bad
    movq $0x99,(%rsi)
    mov (%rdi),%rax
nested_memory_call:
    vmcall
.Lnested_memory_bad:
    ud2
