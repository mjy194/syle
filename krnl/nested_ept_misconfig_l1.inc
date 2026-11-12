/* Only reason49 GPA and guest state are consulted. Qualification, GLA and
 * instruction-length fields have no assumed meaning for misconfiguration.
 * Label comparisons are fixture assertions, never ownership classification. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $3,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rsi
    je .Lnested_misconfig_done
    cmp $49,%rax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea .Lnested_misconfig_gpas(%rip),%rdi
    cmp (%rdi,%rsi,8),%rax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea .Lnested_misconfig_fault_offsets(%rip),%rdi
    mov (%rdi,%rsi,8),%rdx
    add %rdi,%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    lea .Lnested_misconfig_slots(%rip),%rdi
    mov (%rdi,%rsi,8),%rbx
    mov $0x8033,%eax
    cmp $2,%rsi
    jne .Lnested_misconfig_fix
    mov $0xb3,%eax
.Lnested_misconfig_fix:
    mov %rax,(%rbx) /* actual L1 EPT12 correction */
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
    lea .Lnested_misconfig_gvas(%rip),%rdi
    mov (%rdi,%rsi,8),%rsi /* restore exact original L2 load operand */
    incq 0xc820
    vmresume /* no VMCS RIP/RFLAGS write: retry faulting load */
    jmp .Lnested_fail
.Lnested_misconfig_done:
    cmp $18,%rax
    jne .Lnested_fail
    cmp $0x42,%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_misconfig_done_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    incq 0xc820
    jmp .Lnested_done
.p2align 3
.Lnested_misconfig_gpas:
    .quad 0xf000,0xe000,0x208000
.Lnested_misconfig_gvas:
    .quad 0xf000,0xe000,0x30000
.Lnested_misconfig_slots:
    .quad 0x13078,0x13070,0x12008
.Lnested_misconfig_fault_offsets:
    .quad nested_misconfig_fault0-.Lnested_misconfig_fault_offsets
    .quad nested_misconfig_fault1-.Lnested_misconfig_fault_offsets
    .quad nested_misconfig_fault2-.Lnested_misconfig_fault_offsets
