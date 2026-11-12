/* Included immediately after nested_l2_entry. All code references are PIC.
 * NP PTE[f000] remains 8002 throughout: handlers skip, never map the page. */
.global nested_pf_route_fault0,nested_pf_route_fault1,nested_pf_route_fault2,nested_pf_route_fault3
.global nested_pf_route_call0,nested_pf_route_call1,nested_pf_route_call2,nested_pf_route_call3
.global nested_pf_route_handler
    xor %r13d,%r13d
    mov $0x22220000,%eax
    mov %rax,%cr2
    lea nested_pf_route_fault0(%rip),%r11
    lea nested_pf_route_call0(%rip),%r12
    xor %r14d,%r14d /* read, supervisor, not present: error 0 */
    mov $0xf000,%eax
    pushq $2
    popfq
nested_pf_route_fault0:
    mov (%rax),%rbx
nested_pf_route_call0:
    vmcall
nested_l2_resume:
    mov $0x22220001,%eax
    mov %rax,%cr2
    lea nested_pf_route_fault1(%rip),%r11
    lea nested_pf_route_call1(%rip),%r12
    mov $2,%r14d /* write, supervisor, not present */
    mov $0xf000,%eax
    pushq $2
    popfq
nested_pf_route_fault1:
    mov %rbx,(%rax)
nested_pf_route_call1:
    vmcall
    mov $0x22220002,%eax
    mov %rax,%cr2
    lea nested_pf_route_fault2(%rip),%r11
    lea nested_pf_route_call2(%rip),%r12
    mov $2,%r14d
    mov $0xf000,%eax
    pushq $2
    popfq
nested_pf_route_fault2:
    mov %rbx,(%rax)
nested_pf_route_call2:
    vmcall
    mov $0x22220003,%eax
    mov %rax,%cr2
    lea nested_pf_route_fault3(%rip),%r11
    lea nested_pf_route_call3(%rip),%r12
    xor %r14d,%r14d
    mov $0xf000,%eax
    pushq $2
    popfq
nested_pf_route_fault3:
    mov (%rax),%rbx
nested_pf_route_call3:
    vmcall
    ud2
nested_pf_route_handler:
    cmp %r14,(%rsp)
    jne .Lnested_pf_route_bad
    cmp %r11,8(%rsp)
    jne .Lnested_pf_route_bad
    cmpq $0x9000,32(%rsp) /* error,RIP,CS,RFLAGS,RSP,SS */
    jne .Lnested_pf_route_bad
    mov %cr2,%rax
    cmp $0xf000,%rax
    jne .Lnested_pf_route_bad
    inc %r13
    mov %r12,8(%rsp)
    add $8,%rsp
    iretq
.Lnested_pf_route_bad:
    ud2
/* Existing linker/profile compatibility names; not dispatch identities. */
.global nested_event_first_call,nested_event_finish,nested_gp_fault
.global nested_ud_handler,nested_gp_handler
.set nested_event_first_call,nested_pf_route_call0
.set nested_event_finish,nested_pf_route_call3
.set nested_gp_fault,nested_pf_route_fault0
.set nested_ud_handler,nested_pf_route_handler
.set nested_gp_handler,nested_pf_route_handler
