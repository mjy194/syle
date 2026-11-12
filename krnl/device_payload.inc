    cli
    xor %r15,%r15
    mov $0x1b,%ecx
    mov $0xfee00d00,%eax
    xor %edx,%edx
    wrmsr
    mov $0x80f,%ecx
    mov $0x1ff,%eax
    wrmsr
    mov $0xe000,%ebx
    mov $0x10,%eax
    mov %eax,(%rbx)
    mov $0x40,%eax
    mov %eax,0x10(%rbx)
    mov 0x10(%rbx),%eax
    cmp $0x40,%eax
    jne .Lfail
    /* Same store encoding, different RIP: register selector isn't scripted. */
    mov $0x12,%eax
    mov %eax,(%rbx)
    mov $0x18040,%eax
    mov %eax,0x10(%rbx)
    mov $0x100,%eax
    vmcall /* explicit fixture-only edge line stimulus */
    sti
    nop
.Lio_edge_wait:
    cmp $1,%r15
    jne .Lio_edge_wait
    cli
    mov $0x10,%eax
    mov %eax,(%rbx)
    mov $0x10040,%eax
    mov %eax,0x10(%rbx)
    mov $0x12,%eax
    mov %eax,(%rbx)
    mov $0x8040,%eax
    mov %eax,0x10(%rbx)
    mov $0x200,%eax
    vmcall /* explicit fixture-only assert level line */
    sti
    nop
.Lio_level_wait:
    cmp $3,%r15
    jne .Lio_level_wait
    cli
    mov 0x10(%rbx),%eax
    cmp $0x8040,%eax /* remote IRR cleared after final EOI */
    jne .Lfail
    mov $0x400,%eax
    vmcall
    ud2
.global ept_apic_halt,ept_apic_return,ept_apic_ack,ept_apic_handler,ept_apic_first_isr
/* Aliases only for old unreachable diagnostic branch link compatibility. */
ept_apic_halt:
ept_apic_return:
ept_apic_ack:
    ud2
ept_apic_handler:
    push %rax
    push %rcx
    push %rdx
    mov $0x812,%ecx
ept_apic_first_isr:
    rdmsr
    cmp $1,%eax
    jne .Lfail
    inc %r15
    cmp $3,%r15
    jne .Lio_eoi
    mov $0x300,%eax
    vmcall /* deassert before last EOI: no further level delivery */
.Lio_eoi:
    xor %eax,%eax
    xor %edx,%edx
    mov $0x80b,%ecx
    wrmsr
    pop %rdx
    pop %rcx
    pop %rax
    iretq
