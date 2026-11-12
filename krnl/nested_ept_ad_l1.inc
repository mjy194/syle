/* Validate actual EPT12 A/D writeback at each native L2 checkpoint. */
    mov %rax,%r8
    mov 0xc820,%rsi
    cmp $3,%rsi
    ja .Lnested_fail
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%rax
    jne .Lnested_fail
    mov $0x681e,%ecx
    vmread %rcx,%r9
    jbe .Lnested_fail
    lea .Lnested_ad_call_offsets(%rip),%rdi
    mov (%rdi,%rsi,8),%rdx
    add %rdi,%rdx
    cmp %rdx,%r9
    jne .Lnested_fail
    test $1,%sil
    jnz .Lnested_ad_value_checked
    mov $0x11,%edx
    test %rsi,%rsi
    jz .Lnested_ad_value_ready
    mov $0x77,%edx
.Lnested_ad_value_ready:
    cmp %rdx,%r8
    jne .Lnested_fail
.Lnested_ad_value_checked:
    mov 0x10000,%rax
    and $0x300,%eax
    cmp $0x100,%eax
    jne .Lnested_fail
    mov 0x11000,%rax
    and $0x300,%eax
    cmp $0x100,%eax
    jne .Lnested_fail
    mov 0x12000,%rax
    and $0x300,%eax
    cmp $0x100,%eax
    jne .Lnested_fail
    mov 0x12008,%rax
    and $0x300,%eax
    mov $0x100,%edx
    test $1,%sil
    jz .Lnested_ad_data_mask_ready
    mov $0x300,%edx
.Lnested_ad_data_mask_ready:
    cmp %edx,%eax
    jne .Lnested_fail
    mov 0x13020,%rax /* EPT mapping for guest paging-structure page4000 */
    and $0x300,%eax
    cmp $0x300,%eax
    jne .Lnested_fail
    incq 0xc820
    cmp $3,%rsi
    je .Lnested_done
    cmp $1,%rsi
    jne .Lnested_ad_no_clear
    /* L1 clears A/D throughout its own tables and explicitly invalidates.
     * No translation is usable concurrently with these serial L1 edits. */
    mov $0x10000,%edi
    mov $(4*4096/8),%ecx
.Lnested_ad_clear:
    andq $-769,(%rdi) /* clear 0x300 without changing address/permissions */
    add $8,%rdi
    dec %ecx
    jnz .Lnested_ad_clear
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
.Lnested_ad_no_clear:
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%rax
    jne .Lnested_fail
    lea 3(%r9),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
.p2align 3
.Lnested_ad_call_offsets:
    .quad nested_ad_read_call-.Lnested_ad_call_offsets
    .quad nested_ad_write_call-.Lnested_ad_call_offsets
    .quad nested_ad_reread_call-.Lnested_ad_call_offsets
    .quad nested_ad_rewrite_call-.Lnested_ad_call_offsets
