#ifdef VMX_NESTED_TEST
.section .rodata
#include "nested_payload.inc"
.section .note.GNU-stack,"",@progbits
#else
.section .rodata
.global ept_payload_start,ept_payload_end,ept_normal_call,ept_read_fault,ept_write_fault,ept_done_call
/* Copied byte-for-byte to GPA/GVA 0x6000; no host relocations. */
ept_payload_start:
    mov $0x8000,%rdi
    movq $0x42,(%rdi)
    cmpq $0x42,(%rdi)
    jne .Lfail
    mov $0xe000,%eax
ept_normal_call:
    vmcall
    mov $0x9000,%rdi
ept_read_fault:
    mov (%rdi),%rax
    cmp $0x12345678,%rax
    jne .Lfail
    mov $0xa000,%rdi
ept_write_fault:
    movq $0x77,(%rdi)
    cmpq $0x77,(%rdi)
    jne .Lfail
    mov $0xb000,%eax
    call *%rax
    cmp $0xbeef,%rax
    jne .Lfail
    mov $0xe003,%eax
ept_done_call:
#if defined(VMX_IOAPIC_TEST)
#include "device_payload.inc"
#elif defined(VMX_APIC_TEST)
    cli
    /* Restricted architectural x2APIC register-access diagnostic. */
    mov $1,%eax
    cpuid
    test $(1<<21),%ecx
    jz .Lfail
    test $(1<<9),%edx
    jz .Lfail
    mov $0x1b,%ecx
    rdmsr
    cmp $0xfee00900,%eax
    jne .Lfail
    mov $0xfee00d00,%eax
    xor %edx,%edx
    wrmsr
    mov $0x80f,%ecx
    mov $0x1ff,%eax
    wrmsr
#ifdef VMX_TIMER_TEST
    mov $0x83e,%ecx
    mov $0xb,%eax /* divide by 1 */
    wrmsr
    mov $0x832,%ecx
#ifdef VMX_PERIODIC_TEST
    mov $0x30040,%eax /* masked periodic vector40 */
#else
    mov $0x10040,%eax /* masked one-shot vector40 */
#endif
    wrmsr
    mov $0x838,%ecx
#ifdef VMX_PERIODIC_TEST
    mov $0x1000,%eax
#else
    mov $0x100,%eax
#endif
    wrmsr
#ifdef VMX_PERIODIC_TEST
    mov $0x839,%ecx
    rdmsr
    mov %eax,%r12d
.Ltimer_mask_poll:
    rdmsr
    cmp %r12d,%eax
    ja .Ltimer_mask_wrapped
    mov %eax,%r12d
    jmp .Ltimer_mask_poll
.Ltimer_mask_wrapped:
    mov $0x838,%ecx
    xor %eax,%eax
    xor %edx,%edx
    wrmsr /* stop before mode/mask reconfiguration */
#else
.Ltimer_mask_poll:
    mov $0x839,%ecx
    rdmsr
    test %eax,%eax
    jnz .Ltimer_mask_poll
#endif
    mov $0x822,%ecx
    rdmsr
    test %eax,%eax
    jne .Lfail
    mov $0x832,%ecx
#ifdef VMX_PERIODIC_TEST
    mov $0x20040,%eax
#else
    mov $0x40,%eax /* unmask does not replay masked expiry */
#endif
    wrmsr
    mov $0x822,%ecx
    rdmsr
    test %eax,%eax
    jne .Lfail
    mov $0x838,%ecx
    mov $0x100000,%eax /* 2^30 host TSC ticks with divide 1 */
    wrmsr
    mov $0x839,%ecx
    rdmsr
    test %eax,%eax
    jz .Lfail
#else
    mov $0x808,%ecx
    mov $0x50,%eax
    wrmsr
    mov $0x83f,%ecx
    mov $0x40,%eax
    wrmsr
    mov $0x822,%ecx
    rdmsr
    cmp $1,%eax
    jne .Lfail
    mov $0x80a,%ecx
    rdmsr
    cmp $0x50,%eax
    jne .Lfail
    mov $0x808,%ecx
    xor %eax,%eax
    wrmsr
#endif
#ifdef VMX_WINDOW_TEST
    mov $0x51,%r14d
#endif
    sti
    nop
#ifdef VMX_MOVSS_TEST
    mov $0x10,%eax
    mov %ax,%ss
.global ept_movss_fault
ept_movss_fault:
#ifdef VMX_COLLISION_TEST
    ud2
#else
    movq $0x55,0xb080
#endif
#endif
.global ept_apic_halt,ept_apic_return,ept_apic_ack,ept_apic_handler
ept_apic_halt:
#ifndef VMX_WINDOW_TEST
    hlt
#endif
ept_apic_return:
#ifdef VMX_WINDOW_TEST
    cmp $0x51,%r14
    jne .Lfail
    mov $0x52,%r14d
#endif
#ifdef VMX_PERIODIC_TEST
    cmp $3,%r15
    ja .Lfail
    jne .Lperiodic_running
    mov $0x838,%ecx
    xor %eax,%eax
    xor %edx,%edx
    wrmsr
    mov $0x839,%ecx
    rdmsr
    test %eax,%eax
    jne .Lfail
    jmp .Lperiodic_checked
.Lperiodic_running:
    mov $0x839,%ecx
    rdmsr
    test %eax,%eax
    jz .Lfail
.Lperiodic_checked:
#else
    cmp $1,%r15
    jne .Lfail
#endif
    mov $0x812,%ecx
    rdmsr
    test %eax,%eax
    jne .Lfail
    mov $0x822,%ecx
    rdmsr
#ifndef VMX_PERIODIC_TEST
    test %eax,%eax
    jne .Lfail
#else
    cmp $1,%eax
    ja .Lfail
#endif
    mov $0xac41,%eax
ept_apic_ack:
    vmcall
#ifdef VMX_PERIODIC_TEST
    jmp ept_apic_halt /* next HLT waits for next phase, no initial-count rewrite */
#else
    jmp .Lfail
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
.global ept_collision_ud_handler,ept_collision_ud_ack
ept_collision_ud_handler:
    push %rax
    lea ept_movss_fault(%rip),%rax
    cmp %rax,8(%rsp)
    jne .Lfail
    lea ept_apic_return(%rip),%rax
    mov %rax,8(%rsp)
    inc %r13
    mov $0xac06,%eax
ept_collision_ud_ack:
    vmcall
    pop %rax
    iretq
#endif
ept_apic_handler:
    push %rax
    push %rcx
    push %rdx
    lea ept_apic_return(%rip),%rax
    cmp %rax,24(%rsp)
    jne .Lfail
    mov $0x812,%ecx
.global ept_apic_first_isr
ept_apic_first_isr:
    rdmsr
    cmp $1,%eax
    jne .Lfail
    mov $0x80a,%ecx
    rdmsr
    cmp $0x40,%eax
    jne .Lfail
    mov $0x80b,%ecx
    xor %eax,%eax
    xor %edx,%edx
    wrmsr
    inc %r15
    pop %rdx
    pop %rcx
    pop %rax
    iretq
#elif defined(VMX_EVENT_TEST)
.global ept_ud_fault,ept_ud_ack,ept_event_halt,ept_ud_handler
ept_ud_fault:
    ud2
    cmp $0xd006,%r12
    jne .Lfail
    mov $0xac06,%eax
ept_ud_ack:
    vmcall
#ifdef VMX_GP_TEST
.global ept_gp_fault,ept_gp_return,ept_gp_ack,ept_gp_handler
#ifdef VMX_STACKPART_TEST
    pushfq
    pop %r14
    mov $0xd020,%rsp
#endif
    mov $0x28,%eax
ept_gp_fault:
    mov %ax,%ds
ept_gp_return:
    cmp $1,%r15
    jne .Lfail
    mov $0xac0d,%eax
ept_gp_ack:
    vmcall
#endif
#ifdef VMX_PF_TEST
.global ept_pf_fault,ept_pf_return,ept_pf_ack,ept_pf_handler
    mov $0xc000,%edi
ept_pf_fault:
    mov (%rdi),%rax
ept_pf_return:
#ifdef VMX_DEMAND_TEST
    cmp $0x42,%rax
    jne .Lfail
    mov 0x4060,%rax
    and $-33,%rax /* A may be set by the successful read; D must stay clear. */
    cmp $0x8003,%rax
    jne .Lfail
#endif
    cmp $1,%r15
    jne .Lfail
    mov %cr2,%rax
    cmp $0xc000,%rax
    jne .Lfail
    mov $0xac0e,%eax
ept_pf_ack:
    vmcall
#endif
    mov $0xc006,%eax
#ifdef VMX_WAKE_TEST
    sti
    nop
#endif
ept_event_halt:
    hlt
#ifdef VMX_WAKE_TEST
.global ept_irq_return,ept_irq_ack,ept_final_halt,ept_irq_handler
ept_irq_return:
    cmp $1,%r14
    jne .Lfail
    mov $0xac40,%eax
ept_irq_ack:
    vmcall
    cli
    mov $0xc040,%eax
ept_final_halt:
    hlt
    jmp .Lfail
ept_irq_handler:
    push %rax
    lea ept_irq_return(%rip),%rax
    cmp %rax,8(%rsp)
    jne .Lfail
    inc %r14
    pop %rax
    iretq
#endif
    jmp .Lfail
#ifdef VMX_GP_TEST
ept_gp_handler:
#ifdef VMX_STACKPART_TEST
    /* No pushes or scratch stack writes: preserve all surrounding canaries. */
    cmpq $0x28,(%rsp)
    jne .Lfail
    lea ept_gp_fault(%rip),%rax
    cmp %rax,8(%rsp)
    jne .Lfail
    cmpq $8,16(%rsp)
    jne .Lfail
    mov 24(%rsp),%rax
    test $0x10000,%eax
    jz .Lfail
    and $-65537,%rax
    cmp %r14,%rax
    jne .Lfail
    cmpq $0xd020,32(%rsp)
    jne .Lfail
    cmpq $0x10,40(%rsp)
    jne .Lfail
    lea ept_gp_return(%rip),%rax
    mov %rax,8(%rsp)
    inc %r15
    add $8,%rsp
    iretq
#else
    push %rax
    cmpq $0x28,8(%rsp)
    jne .Lfail
    lea ept_gp_fault(%rip),%rax
    cmp %rax,16(%rsp)
    jne .Lfail
    lea ept_gp_return(%rip),%rax
    mov %rax,16(%rsp)
    inc %r15
    pop %rax
    add $8,%rsp /* Consume #GP error code, not part of IRETQ frame. */
    iretq
#endif
#endif
#ifdef VMX_PF_TEST
ept_pf_handler:
    push %rax
    cmpq $0,8(%rsp)
    jne .Lfail
    mov %cr2,%rax
    cmp $0xc000,%rax
    jne .Lfail
    lea ept_pf_fault(%rip),%rax
    cmp %rax,16(%rsp)
    jne .Lfail
#ifdef VMX_DEMAND_TEST
    cmpq $0x8002,0x4060
    jne .Lfail
    movq $0x8003,0x4060
    mov $0xc000,%eax
    invlpg (%rax)
    /* Leave saved fault RIP unchanged: retry the original load. */
#else
    lea ept_pf_return(%rip),%rax
    mov %rax,16(%rsp)
#endif
    inc %r15
    pop %rax
    add $8,%rsp
    iretq
#endif
/* #UD has no error code. Long-mode interrupt frame starts with RIP.
 * Preserve all registers except the deliberate r12 acknowledgement and r13 counter. */
ept_ud_handler:
    push %rax
    lea ept_ud_fault(%rip),%rax
    cmp %rax,8(%rsp)
    jne .Lfail
    addq $2,8(%rsp)
    inc %r13
    mov $0xd006,%r12d
    pop %rax
    iretq
#elif !defined(VMX_EXIT_TEST)
    vmcall
#else
    /* CPUID 32-bit input/output semantics, fixed feature policy, unknown leaf. */
    movabs $0xdeadbeef00000000,%rax
    xor %ecx,%ecx
    mov $-1,%rbx
    mov $-1,%rdx
.global ept_cpuid0
ept_cpuid0:
    cpuid
    cmp $1,%rax
    jne .Lfail
    cmp $0x656c7953,%rbx
    jne .Lfail
    cmp $0x656c7953,%rcx
    jne .Lfail
    cmp $0x656c7953,%rdx
    jne .Lfail
    mov $1,%eax
    xor %ecx,%ecx
.global ept_cpuid1
ept_cpuid1:
    cpuid
    test %rax,%rax
    jne .Lfail
    test %rbx,%rbx
    jne .Lfail
    mov $0x80000000,%r8d
    cmp %r8,%rcx
    jne .Lfail
    cmp $0x40,%rdx
    jne .Lfail
    mov $7,%eax
    xor %ecx,%ecx
.global ept_cpuid2
ept_cpuid2:
    cpuid
    or %rbx,%rax
    or %rcx,%rax
    or %rdx,%rax
    jne .Lfail
    mov $0,%eax
    mov $1,%ecx
.global ept_cpuid3
ept_cpuid3:
    cpuid
    or %rbx,%rax
    or %rcx,%rax
    or %rdx,%rax
    jne .Lfail
    mov $0xc0de,%eax
.global ept_halt
ept_halt:
    hlt
#endif
.Lfail:
    ud2
ept_payload_end:
.global ept_exec_start,ept_exec_end
ept_exec_start:
    mov $0xbeef,%eax
    ret
ept_exec_end:
.section .note.GNU-stack,"",@progbits

#endif
