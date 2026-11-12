/* Relocation-free SIPI image; BSP patches physical far targets and GDTR.
 * Fixed configuration offsets below are mirrored with constants in C. */
.section .rodata.smp_trampoline,"a",@progbits
.balign 16
.global smp_trampoline_start,smp_trampoline_end
smp_trampoline_start:
.code16
    cli
    cld
    mov %cs,%ax
    mov %ax,%ds
    mov %ax,%es
    movzwl %ax,%ebx
    shl $4,%ebx
    lgdtl %cs:0x220
    mov %cr0,%eax
    or $1,%eax
    mov %eax,%cr0
    ljmpl *%cs:0x226
.code32
.Lsmp_pm:
    mov $0x10,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    mov $0x20,%eax /* PAE, all other CR4 features remain disabled */
    mov %eax,%cr4
    mov 0x200(%ebx),%eax
    mov %eax,%cr3
    mov $0xc0000080,%ecx
    rdmsr
    or $0x900,%eax /* LME and NXE; BSP has gated both capabilities */
    wrmsr
    /* INIT may leave CD/NW set. Flush before enabling normal caching;
     * establish supervisor write protection together with paging. */
    wbinvd
    mov %cr0,%eax
    and $0x9fffffff,%eax /* clear CD(bit30) and NW(bit29) together */
    or $0x80010001,%eax /* PG | WP | PE */
    mov %eax,%cr0
    ljmpl *0x22c(%ebx)
.code64
.Lsmp_lm:
    mov $0x10,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    xor %eax,%eax
    mov %ax,%fs
    mov %ax,%gs
    lldt %ax
    xor %edx,%edx
    mov $0xc0000100,%ecx
    wrmsr /* FS_BASE */
    inc %ecx
    wrmsr /* GS_BASE */
    inc %ecx
    wrmsr /* KERNEL_GS_BASE */
    mov .Lsmp_config+8(%rip),%rsp
    and $-16,%rsp
    xor %ebp,%ebp
    mov .Lsmp_config+24(%rip),%rdi
    mov .Lsmp_config+16(%rip),%rax
    call *%rax
.Lsmp_stopped:
    cli
    hlt
    jmp .Lsmp_stopped
.org 0x200
.Lsmp_config:
    .long 0 /* CR3 */
    .long 0
    .quad 0 /* stack top */
    .quad 0 /* C entry */
    .quad 0 /* context */
    .word 31 /* temporary GDT limit */
    .long 0 /* temporary GDT physical base */
    .long 0 /* protected mode physical entry */
    .word 8
    .long 0 /* long mode physical entry */
    .word 24
.org 0x240
.Lsmp_boot_gdt:
    .quad 0,0x00cf9b000000ffff,0x00cf93000000ffff,0x00af9b000000ffff
smp_trampoline_end:
/* Offsets stored as values, not relocations inside copied image. */
.global smp_pm_offset,smp_lm_offset
smp_pm_offset: .long .Lsmp_pm-smp_trampoline_start
smp_lm_offset: .long .Lsmp_lm-smp_trampoline_start
.text
.code64
.global smp_load_gdt
smp_load_gdt:
    lgdt (%rdi)
    pushq $8
    lea 1f(%rip),%rax
    push %rax
    lretq
1:  mov $16,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    xor %eax,%eax
    mov %ax,%fs
    mov %ax,%gs
    lldt %ax
    ret
/* Error-code and non-error-code stubs normalize vector,error,RIP. Each
 * interrupt gate uses a private IST, even when interrupted CPL remains 0. */
.macro AP_STUB n err=0
.Lsmp_exception_\n:
.if !\err
    pushq $0
.endif
    pushq $\n
    jmp .Lsmp_exception_common
.endm
AP_STUB 0,0
AP_STUB 1,0
AP_STUB 2,0
AP_STUB 3,0
AP_STUB 4,0
AP_STUB 5,0
AP_STUB 6,0
AP_STUB 7,0
AP_STUB 8,1
AP_STUB 9,0
AP_STUB 10,1
AP_STUB 11,1
AP_STUB 12,1
AP_STUB 13,1
AP_STUB 14,1
AP_STUB 15,0
AP_STUB 16,0
AP_STUB 17,1
AP_STUB 18,0
AP_STUB 19,0
AP_STUB 20,0
AP_STUB 21,1
AP_STUB 22,0
AP_STUB 23,0
AP_STUB 24,0
AP_STUB 25,0
AP_STUB 26,0
AP_STUB 27,0
AP_STUB 28,0
AP_STUB 29,1
AP_STUB 30,1
AP_STUB 31,0
AP_STUB 255,0
.Lsmp_exception_common:
    cli
    cld
    mov (%rsp),%rdi
    mov 8(%rsp),%rsi
    mov 16(%rsp),%rdx
    mov %cr2,%rcx
    and $-16,%rsp
    call smp_cpu_fault_publish
2:  cli
    hlt
    jmp 2b
.section .rodata
.balign 8
.global smp_exception_stubs
smp_exception_stubs:
    .quad .Lsmp_exception_0
    .quad .Lsmp_exception_1
    .quad .Lsmp_exception_2
    .quad .Lsmp_exception_3
    .quad .Lsmp_exception_4
    .quad .Lsmp_exception_5
    .quad .Lsmp_exception_6
    .quad .Lsmp_exception_7
    .quad .Lsmp_exception_8
    .quad .Lsmp_exception_9
    .quad .Lsmp_exception_10
    .quad .Lsmp_exception_11
    .quad .Lsmp_exception_12
    .quad .Lsmp_exception_13
    .quad .Lsmp_exception_14
    .quad .Lsmp_exception_15
    .quad .Lsmp_exception_16
    .quad .Lsmp_exception_17
    .quad .Lsmp_exception_18
    .quad .Lsmp_exception_19
    .quad .Lsmp_exception_20
    .quad .Lsmp_exception_21
    .quad .Lsmp_exception_22
    .quad .Lsmp_exception_23
    .quad .Lsmp_exception_24
    .quad .Lsmp_exception_25
    .quad .Lsmp_exception_26
    .quad .Lsmp_exception_27
    .quad .Lsmp_exception_28
    .quad .Lsmp_exception_29
    .quad .Lsmp_exception_30
    .quad .Lsmp_exception_31
    .quad .Lsmp_exception_255
.section .note.GNU-stack,"",@progbits

.text
.code64
.global smp_ipi_entry
smp_ipi_entry:
    /* Interrupt gate has saved original RFLAGS (including DF/IF), SS/RSP
     * and cleared IF. Dedicated IST4 separates this frame from fault ISTs.
     * Save every GPR before entering C; IRETQ restores the original flags. */
    push %rax
    push %rcx
    push %rdx
    push %rbx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    cld
    mov %rsp,%r12 /* callee-saved frame anchor, original R12 already saved */
    and $-16,%rsp
    call smp_cpu_ipi_dispatch
    mov %r12,%rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rbx
    pop %rdx
    pop %rcx
    pop %rax
    iretq
