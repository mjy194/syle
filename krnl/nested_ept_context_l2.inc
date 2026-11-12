    /* Outer nested_l2_entry is distinct from the resume-loop label. */
    nop
.global nested_l2_resume,nested_context_call
nested_l2_resume:
    mov $0xf000,%edi
    mov (%rdi),%rax
nested_context_call:
    vmcall
    jmp nested_l2_resume
