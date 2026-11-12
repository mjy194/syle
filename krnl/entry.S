.section .text.entry,"ax",@progbits
.global kernel_entry
.type kernel_entry,@function
kernel_entry:
    /* SysV handoff: RDI points at versioned boot_info. Firmware paging is
     * temporarily retained; this stack belongs to the reserved kernel image. */
    cli
    cld
    lea kernel_startup_stack_top(%rip), %rsp
    and $-16, %rsp
    xor %ebp, %ebp
    call kernel_runtime
1:  cli
    hlt
    jmp 1b
.size kernel_entry, .-kernel_entry
.section .bss,"aw",@nobits
.balign 16
kernel_startup_stack:
    .skip 32768
kernel_startup_stack_top:
.section .note.GNU-stack,"",@progbits
