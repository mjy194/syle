.text
.global source_prepare_pause
.type source_prepare_pause,@function
source_prepare_pause:
 mov $10000000,%ecx
.global source_prepare_window_start
source_prepare_window_start:
 pause
 dec %ecx
 jnz source_prepare_window_start
.global source_prepare_window_end
source_prepare_window_end:
 ret
.size source_prepare_pause,.-source_prepare_pause
.section .note.GNU-stack,"",@progbits
