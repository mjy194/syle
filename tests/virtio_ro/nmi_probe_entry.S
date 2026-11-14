.text
.global _start
_start:
 and $-16,%rsp
 call nmi_probe_main
 ud2
.section .note.GNU-stack,"",@progbits
