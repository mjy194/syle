/* Per-call VMX entry frame: no shared scratch, stack or saved register state.
 * Frame ABI: RAX RBX RCX RDX RSI RDI RBP R8..R15, guestCR2, hostCR2.
 * Guest RSP is exclusively in the VMCS. Build with SysV x86-64 ABI. */
.text
.code64
.global nested_vcpu_enter
.type nested_vcpu_enter,@function
nested_vcpu_enter:
    pushfq
    cli
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    sub $24,%rsp
    mov %rdi,0(%rsp)
    mov %esi,8(%rsp)
    movl $3,16(%rsp)
    mov %cr2,%rax
    mov %rax,128(%rdi)
    mov %rsp,%rax
    mov $0x6c14,%ecx /* HOST_RSP */
    vmwrite %rax,%rcx
    jbe .Lnested_vcpu_restore
    lea .Lnested_vcpu_exit(%rip),%rax
    mov $0x6c16,%ecx /* HOST_RIP */
    vmwrite %rax,%rcx
    jbe .Lnested_vcpu_restore
    mov 0(%rsp),%r15
    mov 120(%r15),%rax
    mov %rax,%cr2
    mov 0(%r15),%rax
    mov 8(%r15),%rbx
    mov 16(%r15),%rcx
    mov 24(%r15),%rdx
    mov 32(%r15),%rsi
    mov 40(%r15),%rdi
    mov 48(%r15),%rbp
    mov 56(%r15),%r8
    mov 64(%r15),%r9
    mov 72(%r15),%r10
    mov 80(%r15),%r11
    mov 88(%r15),%r12
    mov 96(%r15),%r13
    mov 104(%r15),%r14
    mov 112(%r15),%r15
    cmpl $0,8(%rsp)
    jne .Lnested_vcpu_resume
    vmlaunch
    jmp .Lnested_vcpu_vmfail
.Lnested_vcpu_resume:
    vmresume
.Lnested_vcpu_vmfail:
    jc .Lnested_vcpu_invalid
    jz .Lnested_vcpu_valid
    movl $3,16(%rsp)
    jmp .Lnested_vcpu_save
.Lnested_vcpu_invalid:
    movl $1,16(%rsp)
    jmp .Lnested_vcpu_save
.Lnested_vcpu_valid:
    movl $2,16(%rsp)
    jmp .Lnested_vcpu_save
.Lnested_vcpu_exit:
    /* VM-exit reloads the per-call HOST_RSP. Store immediate status before
     * borrowing any register; the guest GPR image remains fully intact. */
    movl $0,16(%rsp)
.Lnested_vcpu_save:
    push %rax
    mov 8(%rsp),%rax /* original local frame pointer, not guest RSP */
    mov %rbx,8(%rax)
    mov %rcx,16(%rax)
    mov %rdx,24(%rax)
    mov %rsi,32(%rax)
    mov %rdi,40(%rax)
    mov %rbp,48(%rax)
    mov %r8,56(%rax)
    mov %r9,64(%rax)
    mov %r10,72(%rax)
    mov %r11,80(%rax)
    mov %r12,88(%rax)
    mov %r13,96(%rax)
    mov %r14,104(%rax)
    mov %r15,112(%rax)
    mov (%rsp),%rcx
    mov %rcx,0(%rax)
    mov %cr2,%rcx
    mov %rcx,120(%rax)
    mov 128(%rax),%rcx
    mov %rcx,%cr2
    add $8,%rsp
.Lnested_vcpu_restore:
    mov 16(%rsp),%eax
    add $24,%rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    popfq
    ret
.size nested_vcpu_enter,.-nested_vcpu_enter

.section .note.GNU-stack,"",@progbits
