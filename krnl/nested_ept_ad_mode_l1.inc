/* Same EPT12 root with explicit, architecturally ordered A/D mode switches.
 * Stages are zero-based; the fourth checkpoint (stage3) proves AD0 silence. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $5,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    lea .Lnested_ad_mode_values(%rip),%rdi
    cmp (%rdi,%rsi,8),%r8
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    lea nested_ad_mode_call(%rip),%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    lea .Lnested_ad_mode_leaves(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    cmp %rax,0x13078
    jne .Lnested_fail
    cmp $3,%rsi
    je .Lnested_ad_mode_check_zero
    /* AD1 stages set both A/D on guest paging-structure accesses. The
     * first AD0 read retains those inherited bits rather than clearing them. */
    mov 0x13020,%rax
    and $0x300,%eax
    cmp $0x300,%eax
    jne .Lnested_fail
    jmp .Lnested_ad_mode_checked
.Lnested_ad_mode_check_zero:
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
.Lnested_ad_mode_zero_scan:
    testq $0x300,(%rdi)
    jnz .Lnested_fail
    add $8,%rdi
    dec %ecx
    jnz .Lnested_ad_mode_zero_scan
.Lnested_ad_mode_checked:
    incq 0xc820
    cmp $5,%rsi
    je .Lnested_done
    cmp $2,%rsi
    jne .Lnested_ad_mode_no_clear
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
.Lnested_ad_mode_clear:
    andq $-769,(%rdi)
    add $8,%rdi
    dec %ecx
    jnz .Lnested_ad_mode_clear
.Lnested_ad_mode_no_clear:
    /* Three invalidations: before AD1->AD0, after clearing in AD0,
     * and before AD0->AD1. The descriptor names the current EPTP. */
    cmp $1,%rsi
    jb .Lnested_ad_mode_resume
    cmp $3,%rsi
    ja .Lnested_ad_mode_resume
    mov $0x1001e,%eax
    cmp $1,%rsi
    jne .Lnested_ad_mode_descriptor
    mov $0x1005e,%eax
.Lnested_ad_mode_descriptor:
    mov %rax,0xc830
    movq $0,0xc838
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_ad_mode_resume:
    lea .Lnested_ad_mode_next(%rip),%rdi
    mov (%rdi,%rsi,8),%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    /* Restore actual L2 load/store controls after all VMX operations. */
    xor %r10d,%r10d
    xor %r11d,%r11d
    test $1,%sil
    jnz .Lnested_ad_mode_enter
    mov $1,%r10d
    lea .Lnested_ad_mode_store_values(%rip),%rdi
    mov (%rdi,%rsi,8),%r11
.Lnested_ad_mode_enter:
    vmresume
    jmp .Lnested_fail
.p2align 3
.Lnested_ad_mode_values:
    .quad 0x11,0x77,0x77,0x88,0x88,0x99
.Lnested_ad_mode_leaves:
    .quad 0x8133,0x8333,0x8333,0x8033,0x8133,0x8333
.Lnested_ad_mode_next:
    .quad 0x1005e,0x1001e,0x1001e,0x1005e,0x1005e
.Lnested_ad_mode_store_values:
    .quad 0x77,0,0x88,0,0x99
