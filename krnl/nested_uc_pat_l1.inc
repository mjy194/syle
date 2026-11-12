/* Two completed calls, no INVEPT. The second return requests an unsafe PAT
 * and deliberately attempts VMRESUME for the monitor's negative test. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $1,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    mov $0x77,%edx
    test %rsi,%rsi
    jz .Lnested_uc_pat_value_ready
    mov $0x99,%edx
.Lnested_uc_pat_value_ready:
    cmp %rdx,%r8
    jne .Lnested_fail
    mov $0xf000,%edi
    cmp %rdx,(%rdi)
    jne .Lnested_fail
    mov $0x2804,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    movabs $0x0606060606040706,%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%r9
    jbe .Lnested_fail
    lea nested_uc_warm_call(%rip),%rdx
    test %rsi,%rsi
    jz .Lnested_uc_pat_rip_ready
    lea nested_uc_done_call(%rip),%rdx
.Lnested_uc_pat_rip_ready:
    cmp %rdx,%r9
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    lea 3(%r9),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    incq 0xc820
    test %rsi,%rsi
    jz .Lnested_uc_pat_resume
    /* Slot7 WC is intentionally outside the admitted virtual PAT contract.
     * No success marker, VMXOFF, or host VMCALL precedes the real attempt. */
    movabs $0x0106060606040706,%rax
    mov $0x2804,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
.Lnested_uc_pat_resume:
    vmresume
    jmp .Lnested_fail
