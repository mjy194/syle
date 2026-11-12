    mov $0x4002,%ecx
    mov $1,%eax /* reserved primary execution-control bit0 */
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmlaunch
    jc .Lnested_fail
    jnz .Lnested_fail
    mov $0x4400,%ecx
    vmread %rcx,%rax
    cmp $7,%eax
    jne .Lnested_fail
    mov $0x4002,%ecx
    xor %eax,%eax
    vmwrite %rax,%rcx
    mov $0xc02,%ecx
    vmwrite %rax,%rcx /* virtual host CS=0 */
    vmlaunch
    jc .Lnested_fail
    jnz .Lnested_fail
    mov $0x4400,%ecx
    vmread %rcx,%rax
    cmp $8,%eax
    jne .Lnested_fail
    mov $0xc02,%ecx
    mov $8,%eax
    vmwrite %rax,%rcx
    mov $0x6820,%ecx
    xor %eax,%eax
    vmwrite %rax,%rcx /* sole intentional physical guest-state violation */
    movabs $0x123456789abcdef0,%r8
    movabs $0xfedcba9876543210,%r9
    vmlaunch
