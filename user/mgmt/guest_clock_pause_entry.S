.section .text
.global _start
_start:
 and $-16,%rsp
 call mg_clock_pause_main
 ud2
.section .note.GNU-stack,"",@progbits
