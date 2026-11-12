.section .rodata
.global mgmt_image_start, mgmt_image_end
mgmt_image_start:
.incbin "build/mgmt.bin"
mgmt_image_end:
.section .note.GNU-stack,"",@progbits
