    mov $0,%r13
    ud2 /* L1 bitmap clear: only L0 intercepts; guest handler must run */
.global nested_event_first_call,nested_event_finish,nested_ud_handler
nested_event_first_call:
    vmcall
nested_l2_resume:
#ifdef VMX_NESTED_GP_INJECT_TEST
#ifdef VMX_NESTED_PF_INJECT_TEST
    mov %cr2,%rax
    cmp $0x11110000,%rax
    jne .Lnested_gp_bad
    mov $0x22220000,%eax
    mov %rax,%cr2
    mov $0xf000,%eax
    /* restore the fixed profile flags without changing fault address */
    pushq $2
    popfq
#else
    mov $0x28,%eax
#endif
.global nested_gp_fault
nested_gp_fault:
#ifdef VMX_NESTED_PF_INJECT_TEST
    mov (%rax),%rax
#else
    mov %ax,%ds /* GDT limit39: selector0x28 out of bounds, real #GP(0x28) */
#endif
#else
    ud2 /* L1 bitmap set: N5 skips; N6 injects to the handler */
#endif
nested_event_finish:
    vmcall
    ud2
nested_ud_handler:
    inc %r13
    addq $2,(%rsp)
    iretq

#ifdef VMX_NESTED_GP_INJECT_TEST
.global nested_gp_handler
nested_gp_handler:
#ifdef VMX_NESTED_PF_INJECT_TEST
    mov %cr2,%rax
    cmp $0xf000,%rax
    jne .Lnested_gp_bad
    cmpq $0,(%rsp)
#else
    cmpq $0x28,(%rsp)
#endif
    jne .Lnested_gp_bad
    lea nested_gp_fault(%rip),%rax
    cmp %rax,8(%rsp)
    jne .Lnested_gp_bad
    cmpq $0x9000,32(%rsp) /* error,RIP,CS,FLAGS,RSP,SS */
    jne .Lnested_gp_bad
    cmp $1,%r13
    jne .Lnested_gp_bad
    inc %r13
    lea nested_event_finish(%rip),%rax
    mov %rax,8(%rsp)
    add $8,%rsp /* IRETQ does not pop the error-code slot */
    iretq
.Lnested_gp_bad:
    ud2
#endif
