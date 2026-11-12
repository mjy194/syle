/* VMCS12 exit information and stage select the action; RIP labels only
 * assert the bounded diagnostic. Only the explicit write skip advances RIP. */
    mov %rax,%r8 /* preserve final L2 result before VMREAD clobbers RAX */
    mov 0xc820,%rsi
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0,%rsi
    je .Lnested_map_warm
    cmp $1,%rsi
    je .Lnested_map_write
    cmp $2,%rsi
    je .Lnested_map_read
    cmp $3,%rsi
    jne .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    cmp $0x99,%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_map_done_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    incq 0xc820
    jmp .Lnested_done
.Lnested_map_warm:
    cmp $18,%rax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_map_warm_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    mov %rax,%r9
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    mov $0x13000,%ebx
    movq $0x8031,160(%rbx) /* revoke W of the warmed mapping */
    movq $0,168(%rbx) /* revoke warmed second mapping entirely */
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
.Lnested_map_write:
    cmp $48,%rax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0x14000,%rax
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
    lea nested_map_write_fault(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    add $(nested_map_after_write-nested_map_write_fault),%rax
    vmwrite %rax,%rcx /* explicit L1 test policy skips this denied store */
    jbe .Lnested_fail
    incq 0xc820
    vmresume /* no translation change, no INVEPT */
    jmp .Lnested_fail
.Lnested_map_read:
    cmp $48,%rax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0x15000,%rax
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
    lea nested_map_read_fault(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    mov $0x13000,%ebx
    movq $0xa033,168(%rbx) /* remap to a different preseeded backing */
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
    mov $0x15000,%esi /* L2's faulting read uses RSI, preserve operand */
    incq 0xc820
    vmresume /* original fault RIP and flags, retry actual read */
    jmp .Lnested_fail
