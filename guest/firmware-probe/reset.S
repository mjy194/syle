/* Independent, trusted 64KiB ROM fixture. Not an OVMF replacement.
 * Requires UG reset VMCS, ROM at FFFF0000, writable owned low RAM through
 * 80000, intercepted CR/EFER transitions and port E9 diagnostic byte output.
 */
.section .text,"ax"
.code16
.global probe_start,probe_pm,probe_lm,probe_reset
probe_start:
 cli
 movb $0x16,%al
 outb %al,$0xe9
 lgdtl %cs:(gdt_ptr-probe_start)
 movl %cr0,%eax
 orl $1,%eax
 movl %eax,%cr0
 ljmpl $0x08,$probe_pm
.code32
probe_pm:
 movw $0x10,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movl $0x80000,%esp
 movb $0x32,%al
 outb %al,$0xe9
 /* Six pages: PML4, PDPT and four PDs (identity map 0..4GiB). */
 cld
 xorl %eax,%eax
 movl $0x1000,%edi
 movl $0x1800,%ecx
 rep stosl
 movl $0x2003,0x1000
 movl $0x3003,%eax
 movl $0x2000,%edi
 movl $4,%ecx
1: movl %eax,(%edi)
 addl $0x1000,%eax
 addl $8,%edi
 loop 1b
 movl $0x83,%eax
 movl $0x3000,%edi
 movl $2048,%ecx
2: movl %eax,(%edi)
 addl $0x200000,%eax
 addl $8,%edi
 loop 2b
 movl %cr4,%eax
 orl $0x20,%eax
 movl %eax,%cr4
 movl $0x1000,%eax
 movl %eax,%cr3
 movl $0xc0000080,%ecx
 rdmsr
 orl $0x100,%eax
 wrmsr
 movl %cr0,%eax
 andl $0x9fffffff,%eax
 orl $0x80000000,%eax
 movl %eax,%cr0
 ljmpl $0x18,$probe_lm
.code64
probe_lm:
 movb $0x64,%al
 outb %al,$0xe9
 hlt
 jmp probe_lm
.balign 8
gdt:
 .quad 0
 .quad 0x00cf9b000000ffff
 .quad 0x00cf93000000ffff
 .quad 0x00af9b000000ffff
gdt_end:
gdt_ptr:
 .word gdt_end-gdt-1
 .long gdt
.org 0xfff0
.code16
probe_reset:
 jmp probe_start
 .fill 0x10000-(.-probe_start),1,0x90
.section .note.GNU-stack,"",@progbits
