/* L1 consumes reflected VMCS12 information, never routes by fixed RIP.
 * Preserve r11/r12/r14: the suspended L2 handler contract uses them. */
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    test %rax,%rax
    jz .Lnested_pf_route_owned
    cmp $18,%rax
    jne .Lnested_fail
    mov 0xc820,%rsi
    cmp $3,%rsi
    ja .Lnested_fail
    lea 1(%rsi),%rdx
    cmp %rdx,%r13
    jne .Lnested_fail
    mov %cr2,%rax
    cmp $0xf000,%rax
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    mov $0x4016,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    test $0x80000000,%eax
    jnz .Lnested_fail
    cmp $2,%rsi
    jb .Lnested_pf_route_valid_checked
    cmp $0xb0e,%eax
    jne .Lnested_fail
.Lnested_pf_route_valid_checked:
    incq 0xc820
    cmp $3,%rsi
    je .Lnested_done
    /* Completed A -> B bitmap0; B -> C bitmap4000; C -> D bitmap0. */
    xor %eax,%eax
    cmp $1,%rsi
    jne .Lnested_pf_route_bitmap_ready
    mov $0x4000,%eax
.Lnested_pf_route_bitmap_ready:
    mov $0x4004,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    add $3,%rax /* VMCALL only: not the faulting instruction */
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.Lnested_pf_route_owned:
    mov 0xc820,%rsi
    cmp $2,%rsi
    jb .Lnested_fail
    cmp $3,%rsi
    ja .Lnested_fail
    cmp %rsi,%r13 /* no L0 handler has secretly consumed this fault */
    jne .Lnested_fail
    lea 0x22220000(%rsi),%rdx
    mov %cr2,%rax
    cmp %rdx,%rax
    jne .Lnested_fail
    mov $0x4404,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    mov $0x80000b0e,%edx
    cmp %rdx,%rax
    jne .Lnested_fail
    mov $0x4406,%ecx
    vmread %rcx,%rdi
    jbe .Lnested_fail
    xor %edx,%edx
    cmp $3,%rsi
    je .Lnested_pf_route_error_ready
    mov $2,%edx
.Lnested_pf_route_error_ready:
    cmp %rdx,%rdi
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp %r11,%rax /* assertion only: no routing or skip decision */
    jne .Lnested_fail
    mov $0x6400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0xf000,%rax
    jne .Lnested_fail
    mov %rax,%cr2 /* L1 supplies the observed fault GVA for injection */
    mov $0x4016,%ecx
    mov $0x80000b0e,%eax
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    mov $0x4018,%ecx
    vmwrite %rdi,%rcx
    jbe .Lnested_fail
    mov $0x401a,%ecx
    xor %eax,%eax
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume /* same fault RIP; guest handler explicitly skips it */
    jmp .Lnested_fail
