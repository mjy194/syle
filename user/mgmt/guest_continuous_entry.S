.section .text
.global _start
_start:
    and $-16,%rsp
    call mg_continuous_main
    ud2
.section .note.GNU-stack,"",@progbits
