.text
.global lvn_fx_user_probe
.type lvn_fx_user_probe,@function
lvn_fx_user_probe:
 push %r12
 push %r13
 sub $520,%rsp
 mov %rdi,%r12
 mov %rsi,%r13
 xor %edi,%edi
 mov %rsp,%rsi
 call lvn_save_raw
 xor %edi,%edi
 mov %r12,%rsi
 call lvn_load_raw
 xor %edi,%edi
 mov %r13,%rsi
 call lvn_save_raw
 xor %edi,%edi
 mov %rsp,%rsi
 call lvn_load_raw
 add $520,%rsp
 pop %r13
 pop %r12
 ret
.size lvn_fx_user_probe,.-lvn_fx_user_probe
.section .note.GNU-stack,"",@progbits
