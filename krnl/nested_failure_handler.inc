    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%eax
    je .Lnested_done
    mov $0x80000021,%edx
    cmp %rdx,%rax
    jne .Lnested_fail
    movabs $0x123456789abcdef0,%rax
    cmp %rax,%r8
    jne .Lnested_fail
    movabs $0xfedcba9876543210,%rax
    cmp %rax,%r9
    jne .Lnested_fail
    mov $0x4400,%ecx
    vmread %rcx,%rax
    cmp $8,%eax /* late failure must not overwrite prior instruction error */
    jne .Lnested_fail
    mov $0x6820,%ecx
    vmread %rcx,%rax
    test %rax,%rax /* no guest-state save; invalid value remains */
    jnz .Lnested_fail
    vmresume
    jc .Lnested_fail
    jnz .Lnested_fail
    mov $0x4400,%ecx
    vmread %rcx,%rax
    cmp $5,%eax /* failed VMLAUNCH kept launch state clear */
    jne .Lnested_fail
    mov $0x6820,%ecx
    mov $2,%eax
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmlaunch /* corrected initial launch, not resume */
    jmp .Lnested_fail
