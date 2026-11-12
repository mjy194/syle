/* VMCS12 exit information and stage select the action; RIP labels only
 * assert the bounded diagnostic. Only the explicit write skip advances RIP. */
    mov %rax,%r8 /* preserve final L2 result before VMREAD clobbers RAX */
    mov 0xc820,%rsi
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0,%rsi
    je .Lnested_large_warm
    cmp $1,%rsi
    je .Lnested_large_write
    cmp $2,%rsi
    je .Lnested_large_read
    cmp $3,%rsi
    jne .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    cmp $0x22,%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_large_done_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    incq 0xc820
    jmp .Lnested_done
.Lnested_large_warm:
    cmp $18,%rax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_large_warm_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    mov %rax,%r9
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    movq $0xb1,0x12008 /* revoke W on the 2-MiB leaf */
    movq $0,0x11008 /* remove the warmed 1-GiB leaf */
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
    incq 0xc820
    lea 3(%r9),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.Lnested_large_write:
    cmp $48,%rax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0x208000,%rax
    jne .Lnested_fail
    mov $0x6400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    and $0x1ff,%eax
    cmp $0x18a,%eax /* direct write, leaf read-only */
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_large_write_fault(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    add $(nested_large_after_write-nested_large_write_fault),%rax
    vmwrite %rax,%rcx /* explicit L1 test policy skips this denied store */
    jbe .Lnested_fail
    incq 0xc820
    vmresume /* no translation change, no INVEPT */
    jmp .Lnested_fail
.Lnested_large_read:
    cmp $48,%rax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0x40009000,%rax
    jne .Lnested_fail
    mov $0x6400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    and $0x1ff,%eax
    cmp $0x181,%eax /* direct read, absent EPT12 leaf */
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_large_read_fault(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    movq $0xb3,0x11008 /* restore original 1-GiB mapping to backing base0 */
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
    mov $0x31000,%esi /* L2's faulting read uses RSI, preserve operand */
    incq 0xc820
    vmresume /* original fault RIP and flags, retry actual read */
    jmp .Lnested_fail
