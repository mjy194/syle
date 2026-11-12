.text
.macro ARM label
 push %rax
 lea \label(%rip),%rax
 mov %rax,lvn_armed_rip(%rip)
 pop %rax
.endm
.macro DISARM
 movq $0,lvn_armed_rip(%rip)
.endm
.global lvn_read_raw,lvn_write_raw,lvn_save_raw,lvn_load_raw
.global lvn_rd_fault,lvn_wr_fault,lvn_fxsave_fault,lvn_fxload_fault,lvn_false
lvn_read_raw:
 mov %esi,%ecx
 mov %rdx,%r8
 ARM lvn_rd_fault
lvn_rd_fault:
 rdmsr
 DISARM
 shl $32,%rdx
 or %rdx,%rax
 mov %rax,(%r8)
 mov $1,%eax
 ret
lvn_write_raw:
 mov %esi,%ecx
 mov %edx,%eax
 shr $32,%rdx
 ARM lvn_wr_fault
lvn_wr_fault:
 wrmsr
 DISARM
 mov $1,%eax
 ret
lvn_save_raw:
 ARM lvn_fxsave_fault
lvn_fxsave_fault:
 fxsave64 (%rsi)
 DISARM
 mov $1,%eax
 ret
lvn_load_raw:
 ARM lvn_fxload_fault
lvn_fxload_fault:
 fxrstor64 (%rsi)
 DISARM
 mov $1,%eax
 ret
lvn_false:
 xor %eax,%eax
 ret
.global lvn_enter_raw
.type lvn_enter_raw,@function
lvn_enter_raw:
 pushfq
 push %rbx
 push %rbp
 push %r12
 push %r13
 push %r14
 push %r15
 push %rdi
 sgdt 160(%rdi)
 sidt 170(%rdi)
 mov %dr6,%rax
 mov %rax,152(%rdi)
 movl $2,140(%rdi)
 mov $0x6c14,%rcx
 mov %rsp,%rax
.global lvn_rsp_fault,lvn_rip_fault,lvn_early_fail
 ARM lvn_rsp_fault
lvn_rsp_fault:
 vmwrite %rax,%rcx
 DISARM
 jbe lvn_early_fail
 mov $0x6c16,%rcx
 lea .Lexit(%rip),%rax
 ARM lvn_rip_fault
lvn_rip_fault:
 vmwrite %rax,%rcx
 DISARM
 jbe lvn_early_fail
 mov %cr2,%rax
 mov %rax,128(%rdi)
 mov 120(%rdi),%rax
 mov %rax,%cr2
 movl $0,140(%rdi)
 cmpl $0,136(%rdi)
 jne .Lresume
 lea .Llaunch(%rip),%rax
 jmp .Lload
.Lresume:
 lea .Lresume_insn(%rip),%rax
.Lload:
 push %rax
 mov %rdi,%rax
 mov 8(%rax),%rbx
 mov 16(%rax),%rcx
 mov 24(%rax),%rdx
 mov 32(%rax),%rsi
 mov 40(%rax),%rdi
 mov 48(%rax),%rbp
 mov 56(%rax),%r8
 mov 64(%rax),%r9
 mov 72(%rax),%r10
 mov 80(%rax),%r11
 mov 88(%rax),%r12
 mov 96(%rax),%r13
 mov 104(%rax),%r14
 mov 112(%rax),%r15
 mov 0(%rax),%rax
 ret
.global lvn_launch_fault,lvn_resume_fault,lvn_entry_fail
.Llaunch:
 ARM lvn_launch_fault
lvn_launch_fault:
 vmlaunch
 DISARM
 jmp lvn_entry_fail
.Lresume_insn:
 ARM lvn_resume_fault
lvn_resume_fault:
 vmresume
 DISARM
lvn_entry_fail:
 mov (%rsp),%rdi
 mov 128(%rdi),%rax
 mov %rax,%cr2
 jmp lvn_early_fail
.Lexit:
 DISARM
 push %rax
 mov 8(%rsp),%rax
 mov %rbx,8(%rax)
 mov %rcx,16(%rax)
 mov %rdx,24(%rax)
 mov %rsi,32(%rax)
 mov %rdi,40(%rax)
 mov %rbp,48(%rax)
 mov %r8,56(%rax)
 mov %r9,64(%rax)
 mov %r10,72(%rax)
 mov %r11,80(%rax)
 mov %r12,88(%rax)
 mov %r13,96(%rax)
 mov %r14,104(%rax)
 mov %r15,112(%rax)
 pop %rdx
 mov %rdx,0(%rax)
 mov %cr2,%rdx
 mov %rdx,120(%rax)
 mov 128(%rax),%rdx
 mov %rdx,%cr2
 movl $1,140(%rax)
 mov $1,%eax
 jmp .Lreturn
lvn_early_fail:
 xor %eax,%eax
.Lreturn:
 mov (%rsp),%rdi
 lgdt 160(%rdi)
 lidt 170(%rdi)
 mov 152(%rdi),%rdx
 mov %rdx,%dr6
 add $8,%rsp
 pop %r15
 pop %r14
 pop %r13
 pop %r12
 pop %rbp
 pop %rbx
 popfq
 ret
.size lvn_enter_raw,.-lvn_enter_raw
.section .note.GNU-stack,"",@progbits
