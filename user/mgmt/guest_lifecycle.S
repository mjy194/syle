/* Opt-in Ring3 lifecycle exercise. No boot-success claim; built separately
 * from the ordinary manager. Requires a real registered backend. */
.section .text
.global _start
_start:
    mov $16,%eax                 /* create fixed kernel-selected profile */
    xor %edi,%edi
    mov $1,%esi
    int $0x80
    test %rax,%rax
    jle fail
    mov %rax,%r12
    mov %r12,%rdi
    xor %esi,%esi
    mov $17,%eax                 /* initial bounded slice */
    int $0x80
    test %rax,%rax
    jne cleanup_fail
    mov $18,%eax                 /* preserve VMCS */
    int $0x80
    test %rax,%rax
    jne cleanup_fail
    mov $17,%eax                 /* resume the same guest, not a new boot */
    int $0x80
    test %rax,%rax
    jne cleanup_fail
    mov $4,%r13d
step:
    mov $22,%eax
    int $0x80
    test %rax,%rax
    jne cleanup_fail
    dec %r13d
    jne step
    mov $18,%eax
    int $0x80
    test %rax,%rax
    jne cleanup_fail
    mov $20,%eax                 /* teardown VMX before releasing RAM */
    int $0x80
    test %rax,%rax
    jne fail
    mov $1,%eax
    lea message(%rip),%rdi
    mov $message_end-message,%esi
    int $0x80
    xor %edi,%edi
    jmp exit
cleanup_fail:
    mov $20,%eax
    int $0x80                    /* failure remains quarantined, never ignored
                                   as a successful lifecycle */
fail:
    mov $1,%edi
exit:
    mov $3,%eax                  /* kernel independently retries owner cleanup */
    int $0x80
    ud2
message: .ascii "SYLE.MGMT lifecycle slices completed; not Linux boot acceptance\n"
message_end:
.section .note.GNU-stack,"",@progbits
