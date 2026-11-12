/* Position-independent, separately assembled management demonstration.
 * int 0x80 ABI: rax number, rdi buffer, rsi length; rax result.
 * Initial rdi is a diagnostic test selector, rsi a protected kernel address.
 */
.section .text
.global _start
_start:
    cmp $1, %rdi
    je read_kernel
    cmp $2, %rdi
    je write_code
    cmp $3, %rdi
    je exec_stack
    cmp $4, %rdi
    je io_port
    mov $1, %eax
    lea message(%rip), %rdi
    mov $message_end-message, %esi
    int $0x80
    cmp $message_end-message, %rax
    jne fail
    /* Kernel pointers must be rejected rather than dereferenced. */
    mov $1, %eax
    xor %edi, %edi
    mov $1, %esi
    int $0x80
    cmp $-1, %rax
    jne fail
    /* Obtain minimal system status through the explicit syscall boundary. */
    mov $2, %eax
    int $0x80
    cmp $1, %rax
    jne fail
    xor %edi, %edi
    jmp exit
fail:
    mov $1, %edi
exit:
    mov $3, %eax
    int $0x80
    ud2
read_kernel:
    mov (%rsi), %rax
    jmp fail
write_code:
    lea _start(%rip), %rax
    movb $0, (%rax)
    jmp fail
exec_stack:
    movb $0xc3, -8(%rsp)
    lea -8(%rsp), %rax
    jmp *%rax
io_port:
    out %al, $0xf4
    jmp fail
message: .ascii "SYLE.MGMT: ring3 management process online\n"
message_end:
