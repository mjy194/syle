/* Installed only by managed-ept-probe before any guest instructions run. */
.section .rodata,"a",@progbits
.code64
.global lep_code_start,lep_code_end,lep_read,lep_read_done,lep_write,lep_write_done
lep_code_start:
 movabs $0xb000,%rax
lep_read:
 mov (%rax),%rbx
 mov $1,%eax
lep_read_done:
 vmcall
 movabs $0xb000,%rax
 movabs $0x7766554433221100,%rbx
lep_write:
 mov %rbx,(%rax)
 mov $2,%eax
lep_write_done:
 vmcall
 movabs $0xc000,%rax
 jmp *%rax
 ud2
lep_code_end:
.global lep_exec_start,lep_exec_end,lep_exec_done
lep_exec_start:
 movabs $0x123456789abcdef0,%rbx
 mov $3,%eax
lep_exec_done:
 vmcall
 ud2
lep_exec_end:
.section .note.GNU-stack,"",@progbits
