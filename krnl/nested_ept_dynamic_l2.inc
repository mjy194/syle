    nop /* keep nested_l2_entry distinct from the loop entry */
.global nested_l2_resume,nested_dynamic_call
nested_l2_resume:
    mov $0xf000,%edi
    mov (%rdi),%rax
nested_dynamic_call:
    vmcall
    jmp nested_l2_resume
