/* Budget-two A,B,A,C,B,B,A sequence, all accesses use one real L2 site.
 * Context identity and invalidation are selected by actual L1 VMX ops. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $6,%rsi
    ja .Lnested_fail
    cmp %rsi,0xa800
    jne .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    lea 0x77(%rsi),%rdx
    cmp %rdx,%r8
    jne .Lnested_fail
    mov $0xf000,%edi
    cmp %rdx,(%rdi) /* direct L1 UC view of the same private backing */
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%r9
    jbe .Lnested_fail
    lea nested_uc_context_call(%rip),%rdx
    cmp %rdx,%r9
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    cmp $2,%rsi
    jne .Lnested_uc_context_c_ready
    /* VMCS field list in c000 was consumed before VMLAUNCH. Only now may
     * it become C's root. Preserve later lifecycle pointers across zeroing.
     * Its high entries reused as metadata never lie on this guest's paths. */
    mov 0xc810,%r12
    mov 0xc818,%r13
    mov $0xc000,%edi
    mov $512,%ecx
    xor %eax,%eax
.Lnested_uc_context_clear_c:
    mov %rax,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_uc_context_clear_c
    movq $0x11007,0xc000
    mov %r12,0xc810
    mov %r13,0xc818
    movq $0x1001e,0xc830
    movq $0,0xc838
.Lnested_uc_context_c_ready:
    lea 1(%rsi),%rax
    mov %rax,0xc820
    mov %rax,0xa800
    cmp $6,%rsi
    je .Lnested_done
    cmp $4,%rsi
    je .Lnested_uc_context_single
    cmp $5,%rsi
    jne .Lnested_uc_context_select
    movq $0xdead,0xc830 /* ignored descriptor EPTP with type2 */
    movq $0,0xc838
    mov $2,%eax
    jmp .Lnested_uc_context_invalidate
.Lnested_uc_context_single:
    movq $0xb01e,0xc830
    movq $0,0xc838
    mov $1,%eax
.Lnested_uc_context_invalidate:
    mov $0xc830,%ebx
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_uc_context_select:
    lea .Lnested_uc_context_next(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea 3(%r9),%rax /* completed VMCALL, then native loop branch */
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.p2align 3
.Lnested_uc_context_next:
    .quad 0xb01e,0x1001e,0xc01e,0xb01e,0xb01e,0x1001e
