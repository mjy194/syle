/* Eight actual L2 reads at one instruction site. Only the L1 stage machine
 * determines EPTP switches/invalidation, never an L0 fixed-RIP emulation. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $7,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    lea .Lnested_context_expected(%rip),%rdi
    mov (%rdi,%rsi,8),%rdx
    cmp %rdx,%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_context_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    incq 0xc820
    cmp $7,%rsi
    je .Lnested_done
    cmp $1,%rsi
    jne .Lnested_context_no_edit
    /* Both contexts have warmed f000. Change backing without invalidation. */
    movq $0xa033,0x13078
    movq $0xb033,0x17078
.Lnested_context_no_edit:
    cmp $3,%rsi
    jne .Lnested_context_no_single
    /* A-only invalidation while current EPTP is B. */
    movq $0x1001e,0xc830
    movq $0,0xc838
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_context_no_single:
    cmp $5,%rsi
    jne .Lnested_context_no_global
    /* Type2 ignores the descriptor EPTP; its reserved high qword is zero. */
    movq $0xdead,0xc830
    movq $0,0xc838
    mov $0xc830,%ebx
    mov $2,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_context_no_global:
    lea .Lnested_context_next_eptp(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.p2align 3
.Lnested_context_expected:
    .quad 0x11,0x22,0x11,0x22,0x33,0x22,0x44,0x33
.Lnested_context_next_eptp:
    .quad 0x1401e,0x1001e,0x1401e,0x1001e,0x1401e,0x1401e,0x1001e
