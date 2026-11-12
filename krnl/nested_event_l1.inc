#ifdef VMX_NESTED_PF_INJECT_TEST
#define ERROR_INFO 0x80000b0e
#define ERROR_LOWINFO 0xb0e
#define ERROR_BITMAP 0x4000
#define ERROR_VALUE 0
#else
#define ERROR_INFO 0x80000b0d
#define ERROR_LOWINFO 0xb0d
#define ERROR_BITMAP 0x2000
#define ERROR_VALUE 0x28
#endif
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    test %eax,%eax
    jz .Lowned_ud
    cmp $18,%eax
    jne .Lnested_fail
#ifdef VMX_NESTED_INJECT_TEST
    mov $0x4016,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    test $0x80000000,%eax
    jnz .Lnested_fail
    cmpq $1,0xc820
    jne .Linjection_first_return
#ifdef VMX_NESTED_GP_INJECT_TEST
    cmp $ERROR_LOWINFO,%eax
#else
    cmp $0x306,%eax
#endif
    jne .Lnested_fail
    cmp $2,%r13
    jne .Lnested_fail
#ifdef VMX_NESTED_PF_INJECT_TEST
    mov %cr2,%rax
    cmp $0xf000,%rax
    jne .Lnested_fail
#endif
    jmp .Linjection_return_checked
.Linjection_first_return:
    cmp $1,%r13
    jne .Lnested_fail
.Linjection_return_checked:
#else
    cmp $1,%r13
    jne .Lnested_fail
#endif
    mov $0xc820,%ebx
    incq (%rbx)
    cmpq $2,(%rbx)
    je .Lnested_done
    cmpq $1,(%rbx)
    jne .Lnested_fail
    mov $0x4004,%ecx
#ifdef VMX_NESTED_GP_INJECT_TEST
    mov $ERROR_BITMAP,%eax
#else
    mov $0x40,%eax
#endif
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    add $3,%rax
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.Lowned_ud:
#ifdef VMX_NESTED_PF_INJECT_TEST
    /* A direct intercepted PF must leave L2's last CR2 write unchanged.
     * Virtual VM-exit has NOT restored an older L1 CR2. */
    mov %cr2,%rax
    cmp $0x22220000,%rax
    jne .Lnested_fail
    mov $0x6400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0xf000,%rax
    jne .Lnested_fail
    mov %rax,%cr2 /* L1, not L0, prepares CR2 for event injection */
#endif
    cmp $1,%r13 /* second UD was not secretly consumed by L0 */
    jne .Lnested_fail
    mov $0x4404,%ecx
    vmread %rcx,%rax
#ifdef VMX_NESTED_GP_INJECT_TEST
    mov $ERROR_INFO,%edx
#else
    mov $0x80000306,%edx
#endif
    cmp %rdx,%rax
    jne .Lnested_fail
#ifdef VMX_NESTED_INJECT_TEST
    mov $0x4016,%ecx
#ifdef VMX_NESTED_GP_INJECT_TEST
    mov $ERROR_INFO,%eax
#else
    mov $0x80000306,%eax
#endif
    vmwrite %rax,%rcx
    jbe .Lnested_fail
#ifdef VMX_NESTED_GP_INJECT_TEST
    mov $0x4406,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $ERROR_VALUE,%eax
    jne .Lnested_fail
#endif
    mov $0x4018,%ecx
#ifndef VMX_NESTED_GP_INJECT_TEST
    xor %eax,%eax
#endif
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    mov $0x401a,%ecx
    xor %eax,%eax
    vmwrite %rax,%rcx
    jbe .Lnested_fail
#else
    mov $0x681e,%ecx
    vmread %rcx,%rax
    add $2,%rax /* explicit L1 policy decision, not monitor opcode skipping */
    vmwrite %rax,%rcx
    jbe .Lnested_fail
#endif
    vmresume
    jmp .Lnested_fail
