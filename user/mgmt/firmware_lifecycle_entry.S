.section .text
.global _start
_start:
 and $-16,%rsp
 call firmware_lifecycle_main
 ud2
.section .note.GNU-stack,"",@progbits
