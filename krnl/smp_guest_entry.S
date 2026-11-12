/* Per-call VMX entry frame: no shared scratch, stack or saved register state.
 * Frame ABI: RAX RCX RDX RBX RBP RSI RDI R8..R15, guestCR2, hostCR2.
 * Guest RSP is exclusively in the VMCS. Build with SysV x86-64 ABI. */
.text
.code64
.global smp_guest_enter
.type smp_guest_enter,@function
smp_guest_enter:
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
    jbe .Lsmp_guest_restore
    lea .Lsmp_guest_exit(%rip),%rax
    mov $0x6c16,%ecx /* HOST_RIP */
    vmwrite %rax,%rcx
    jbe .Lsmp_guest_restore
    mov 0(%rsp),%r15
    mov 120(%r15),%rax
    mov %rax,%cr2
    mov 0(%r15),%rax
    mov 8(%r15),%rcx
    mov 16(%r15),%rdx
    mov 24(%r15),%rbx
    mov 32(%r15),%rbp
    mov 40(%r15),%rsi
    mov 48(%r15),%rdi
    mov 56(%r15),%r8
    mov 64(%r15),%r9
    mov 72(%r15),%r10
    mov 80(%r15),%r11
    mov 88(%r15),%r12
    mov 96(%r15),%r13
    mov 104(%r15),%r14
    mov 112(%r15),%r15
    cmpl $0,8(%rsp)
    jne .Lsmp_guest_resume
    vmlaunch
    jmp .Lsmp_guest_vmfail
.Lsmp_guest_resume:
    vmresume
.Lsmp_guest_vmfail:
    jc .Lsmp_guest_invalid
    jz .Lsmp_guest_valid
    movl $3,16(%rsp)
    jmp .Lsmp_guest_save
.Lsmp_guest_invalid:
    movl $1,16(%rsp)
    jmp .Lsmp_guest_save
.Lsmp_guest_valid:
    movl $2,16(%rsp)
    jmp .Lsmp_guest_save
.Lsmp_guest_exit:
    /* VM-exit reloads the per-call HOST_RSP. Store immediate status before
     * borrowing any register; the guest GPR image remains fully intact. */
    movl $0,16(%rsp)
.Lsmp_guest_save:
    push %rax
    mov 8(%rsp),%rax /* original local frame pointer, not guest RSP */
    mov %rcx,8(%rax)
    mov %rdx,16(%rax)
    mov %rbx,24(%rax)
    mov %rbp,32(%rax)
    mov %rsi,40(%rax)
    mov %rdi,48(%rax)
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
.Lsmp_guest_restore:
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
.size smp_guest_enter,.-smp_guest_enter
.section .rodata.smp_guest_payload,"a",@progbits
.balign 16
.global smp_guest_payload_start,smp_guest_payload_end,smp_guest_payload_call
smp_guest_payload_start:
    /* Use each vCPU's private VMCS stack, preserving its unique R8 token. */
    push %r8
    pop %r9
    mov $0x8000,%edi
    mov (%rdi),%rax
smp_guest_payload_call:
    vmcall
    /* PIC repeated iterations: the monitor advances past VMCALL by 3
     * before VMRESUME. Each iteration rechecks private stack and data.
     * N21's single-entry diagnostic still stops at the first VMCALL. */
    jmp smp_guest_payload_start
smp_guest_payload_end:
.global smp_guest_payload_call_offset
smp_guest_payload_call_offset:
    .long smp_guest_payload_call-smp_guest_payload_start
.section .note.GNU-stack,"",@progbits
/* Separate trusted diagnostic: AP remains in guest until a shared-memory
 * release; no STI and no implication of arbitrary interrupt delivery. */
.section .rodata.smp_guest_running_payload,"a",@progbits
.balign 16
.global smp_guest_running_payload_start,smp_guest_running_payload_end
.global smp_guest_running_payload_call,smp_guest_running_payload_call_offset
smp_guest_running_payload_start:
    push %r8
    pop %r9
    mov $0x8000,%edi
    mov (%rdi),%rax
    test %r10,%r10
    jz smp_guest_running_payload_call
    mov $0x7000,%esi
    mov %rax,(%rsi) /* publish old data before heartbeat on x86 WB RAM */
    movq $1,8(%rsi)
.Lsmp_guest_running_wait:
    pause
    cmpq $0,16(%rsi)
    je .Lsmp_guest_running_wait
.Lsmp_guest_running_wait_end:
    mov (%rdi),%rax /* actual reload after the monitor releases this AP */
smp_guest_running_payload_call:
    vmcall
    /* BSP may resume past VMCALL to repeat the read with a fresh mapping.
     * AP is stopped by the monitor at its final VMCALL, before this jump. */
    jmp smp_guest_running_payload_start
smp_guest_running_payload_end:
smp_guest_running_payload_call_offset:
    .long smp_guest_running_payload_call-smp_guest_running_payload_start

.global smp_guest_running_payload_loop_begin_offset,smp_guest_running_payload_loop_end_offset
smp_guest_running_payload_loop_begin_offset:
    .long .Lsmp_guest_running_wait-smp_guest_running_payload_start
smp_guest_running_payload_loop_end_offset:
    .long .Lsmp_guest_running_wait_end-smp_guest_running_payload_start
/* Separate UC diagnostic payload; memory-type admission belongs to the
 * monitor, not this code. Preserve the initial read as R10 evidence. */
.section .rodata.smp_guest_uc_payload,"a",@progbits
.balign 16
.global smp_guest_uc_payload_start,smp_guest_uc_payload_end
.global smp_guest_uc_payload_call,smp_guest_uc_payload_call_offset
smp_guest_uc_payload_start:
    push %r8
    pop %r9
    mov $0x8000,%edi
    mov (%rdi),%r10
    movq $0x77,(%rdi)
    mov (%rdi),%rax
smp_guest_uc_payload_call:
    vmcall
    ud2
smp_guest_uc_payload_end:
smp_guest_uc_payload_call_offset:
    .long smp_guest_uc_payload_call-smp_guest_uc_payload_start
