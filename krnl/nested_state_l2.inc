    mov $0xc850,%ebx
    movw $47,(%rbx)
    movq $0x5000,2(%rbx)
    lgdt (%rbx)
    movw $15,(%rbx)
    movq $0x5900,2(%rbx)
    lidt (%rbx)
    mov $0x8800,%esp
    pushq $0x403
    popfq
    movabs $0x123456789abcdef0,%r8
    movabs $0xfedcba9876543210,%r9
    mov $0x1357,%r10
.global nested_state_first_call,nested_state_second_call
nested_state_first_call:
    vmcall
nested_l2_resume:
    pushfq
    pop %rax
    cmp $0x46,%eax
    jne .Lnested_fail
    cmp $0x8700,%rsp
    jne .Lnested_fail
    mov $0xc880,%ebx
    sgdt (%rbx)
    cmpw $39,(%rbx)
    jne .Lnested_fail
    cmpq $0x5000,2(%rbx)
    jne .Lnested_fail
    sidt (%rbx)
    cmpw $0,(%rbx)
    jne .Lnested_fail
    cmpq $0x5800,2(%rbx)
    jne .Lnested_fail
    mov $0x2468,%r8
    mov $0x3579,%r9
    mov $0x468a,%r10
    pushq $0x46
    popfq
nested_state_second_call:
    vmcall
    ud2
