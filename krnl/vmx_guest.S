.text
/* No C calls while guest GPRs are live. VMCS saves guest RSP/RIP/RFLAGS. */
.macro LOAD_GPRS
    mov vmx_guest_regs+0(%rip), %rax
    mov vmx_guest_regs+8(%rip), %rbx
    mov vmx_guest_regs+16(%rip), %rcx
    mov vmx_guest_regs+24(%rip), %rdx
    mov vmx_guest_regs+32(%rip), %rsi
    mov vmx_guest_regs+40(%rip), %rdi
    mov vmx_guest_regs+48(%rip), %rbp
    mov vmx_guest_regs+56(%rip), %r8
    mov vmx_guest_regs+64(%rip), %r9
    mov vmx_guest_regs+72(%rip), %r10
    mov vmx_guest_regs+80(%rip), %r11
    mov vmx_guest_regs+88(%rip), %r12
    mov vmx_guest_regs+96(%rip), %r13
    mov vmx_guest_regs+104(%rip), %r14
    mov vmx_guest_regs+112(%rip), %r15
.endm
.global vmx_guest_enter
.type vmx_guest_enter,@function
vmx_guest_enter:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    pushfq
    mov $0x6c14,%rcx
    mov %rsp,%rax
    vmwrite %rax,%rcx
    jbe .Lwrite_failed
    mov $0x6c16,%rcx
    lea .Lexit(%rip),%rax
    vmwrite %rax,%rcx
    jbe .Lwrite_failed
#if defined(VMX_PF_TEST) || defined(VMX_NESTED_PF_INJECT_TEST)
    mov %cr2,%rax
    mov %rax,vmx_host_cr2(%rip)
    mov vmx_guest_cr2(%rip),%rax
    mov %rax,%cr2
#endif
    test %edi,%edi
    jnz .Lresume
    LOAD_GPRS
    vmlaunch
    jmp .Lentry_failed
.Lresume:
    LOAD_GPRS
    vmresume
.Lentry_failed:
#if defined(VMX_PF_TEST) || defined(VMX_NESTED_PF_INJECT_TEST)
    mov vmx_host_cr2(%rip),%rax
    mov %rax,%cr2
#endif
    xor %eax,%eax
    jmp .Lreturn
.Lwrite_failed:
    mov $2,%eax
    jmp .Lreturn
.Lexit:
    mov %rax, vmx_guest_regs+0(%rip)
    mov %rbx, vmx_guest_regs+8(%rip)
    mov %rcx, vmx_guest_regs+16(%rip)
    mov %rdx, vmx_guest_regs+24(%rip)
    mov %rsi, vmx_guest_regs+32(%rip)
    mov %rdi, vmx_guest_regs+40(%rip)
    mov %rbp, vmx_guest_regs+48(%rip)
    mov %r8, vmx_guest_regs+56(%rip)
    mov %r9, vmx_guest_regs+64(%rip)
    mov %r10, vmx_guest_regs+72(%rip)
    mov %r11, vmx_guest_regs+80(%rip)
    mov %r12, vmx_guest_regs+88(%rip)
    mov %r13, vmx_guest_regs+96(%rip)
    mov %r14, vmx_guest_regs+104(%rip)
    mov %r15, vmx_guest_regs+112(%rip)
#if defined(VMX_PF_TEST) || defined(VMX_NESTED_PF_INJECT_TEST)
    mov %cr2,%rax
    mov %rax,vmx_guest_cr2(%rip)
    mov vmx_host_cr2(%rip),%rax
    mov %rax,%cr2
#endif
    mov $1,%eax
.Lreturn:
    popfq
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
.size vmx_guest_enter,.-vmx_guest_enter
.global vmx_guest_payload
.type vmx_guest_payload,@function
vmx_guest_payload:
    xor %ebx,%ebx
    mov $0x103, %rcx
    mov $0x104, %rdx
    mov $0x105, %rsi
    mov $0x106, %rdi
    mov $0x107, %rbp
    mov $0x108, %r8
    mov $0x109, %r9
    mov $0x10a, %r10
    mov $0x10b, %r11
    mov $0x10c, %r12
    mov $0x10d, %r13
    mov $0x10e, %r14
    mov $0x10f, %r15
    xor %eax,%eax
    stc
    movabs $0x53594c45564d5831,%rax
.global vmx_guest_vmcall0
vmx_guest_vmcall0:
    vmcall
    /* Confirm the previous VMRESUME restored flags as well as GPRs. */
    pushfq
    pop %rax
    cmp $0x47,%rax
    jne .Lguest_bad_flags
    inc %rbx
    xor %eax,%eax
    stc
    movabs $0x53594c45564d5832,%rax
.global vmx_guest_vmcall1
vmx_guest_vmcall1:
    vmcall
    /* Confirm the previous VMRESUME restored flags as well as GPRs. */
    pushfq
    pop %rax
    cmp $0x47,%rax
    jne .Lguest_bad_flags
    inc %rbx
    xor %eax,%eax
    stc
    movabs $0x53594c45564d5833,%rax
.global vmx_guest_vmcall2
vmx_guest_vmcall2:
    vmcall
    ud2 /* terminal call must never resume */
.Lguest_bad_flags:
    xor %eax,%eax
    vmcall /* wrong RIP/marker: host must reject */
    ud2
.size vmx_guest_payload,.-vmx_guest_payload
.section .note.GNU-stack,"",@progbits
