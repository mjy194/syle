#!/usr/bin/env python3
"""Append read-only context refusal helper to isolated assembly copies only."""
import pathlib,sys
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(sys.argv[1])
helper='''
.section .text.exceptions_fatal_context,"ax",@progbits
.global exceptions_fatal_context
.type exceptions_fatal_context,@function
exceptions_fatal_context:
    movzbl nmi_entered(%rip), %eax
    movzbl exception_entered(%rip), %edx
    orl %edx, %eax
    ret
.size exceptions_fatal_context,.-exceptions_fatal_context
.section .note.GNU-stack,"",@progbits
'''
for name in ['exceptions.S','linux_exceptions.S']:
 s=(repo/'efi/src'/name).read_text()
 assert s.count('nmi_entered: .byte 0')==1 and s.count('exception_entered: .byte 0')==1
 entry=s[s.index('exception_2:'):s.index('.globl exception_3')]
 assert entry.index('movb $1, nmi_entered')<entry.index('tss+52')<entry.index('jmp exception_common')
 assert 'movb $0,nmi_entered' not in s and 'movb $0, nmi_entered' not in s
 p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s+helper)
