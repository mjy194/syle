.text
.globl load_gdt_segments
load_gdt_segments:
    lgdt (%rdi)
    pushq $8
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:  movw $16, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    # Drop firmware selectors before using our own GDT. No kernel TLS/LDT.
    xor %eax, %eax
    movw %ax, %fs
    movw %ax, %gs
    lldt %ax
    ret
.globl exception_0
exception_0:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $0
    jmp exception_common
.globl exception_1
exception_1:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $1
    jmp exception_common
.globl exception_2
exception_2:
    cmpb $0, nmi_entered(%rip)
    jne nmi_emergency_stop
    movb $1, nmi_entered(%rip)
    # Preserve RAX while moving the next hardware frame to a spare IST.
    movq %rax, nmi_saved_rax(%rip)
    leaq nmi_spare_top(%rip), %rax
    movq %rax, tss+52(%rip)
    movq nmi_saved_rax(%rip), %rax
    cmpb $0, exception_entered(%rip)
    jne nmi_emergency_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $2
    jmp exception_common
.globl exception_3
exception_3:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $3
    jmp exception_common
.globl exception_4
exception_4:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $4
    jmp exception_common
.globl exception_5
exception_5:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $5
    jmp exception_common
.globl exception_6
exception_6:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $6
    jmp exception_common
.globl exception_7
exception_7:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $7
    jmp exception_common
.globl exception_8
exception_8:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $8
    jmp exception_common
.globl exception_9
exception_9:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $9
    jmp exception_common
.globl exception_10
exception_10:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $10
    jmp exception_common
.globl exception_11
exception_11:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $11
    jmp exception_common
.globl exception_12
exception_12:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $12
    jmp exception_common
.globl exception_13
exception_13:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $13
    jmp exception_common
.globl exception_14
exception_14:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $14
    jmp exception_common
.globl exception_15
exception_15:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $15
    jmp exception_common
.globl exception_16
exception_16:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $16
    jmp exception_common
.globl exception_17
exception_17:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $17
    jmp exception_common
.globl exception_18
exception_18:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $18
    jmp exception_common
.globl exception_19
exception_19:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $19
    jmp exception_common
.globl exception_20
exception_20:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $20
    jmp exception_common
.globl exception_21
exception_21:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $21
    jmp exception_common
.globl exception_22
exception_22:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $22
    jmp exception_common
.globl exception_23
exception_23:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $23
    jmp exception_common
.globl exception_24
exception_24:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $24
    jmp exception_common
.globl exception_25
exception_25:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $25
    jmp exception_common
.globl exception_26
exception_26:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $26
    jmp exception_common
.globl exception_27
exception_27:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $27
    jmp exception_common
.globl exception_28
exception_28:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $28
    jmp exception_common
.globl exception_29
exception_29:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $29
    jmp exception_common
.globl exception_30
exception_30:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $30
    jmp exception_common
.globl exception_31
exception_31:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $31
    jmp exception_common
.globl exception_32
exception_32:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $32
    jmp exception_common
.globl exception_33
exception_33:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $33
    jmp exception_common
.globl exception_34
exception_34:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $34
    jmp exception_common
.globl exception_35
exception_35:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $35
    jmp exception_common
.globl exception_36
exception_36:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $36
    jmp exception_common
.globl exception_37
exception_37:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $37
    jmp exception_common
.globl exception_38
exception_38:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $38
    jmp exception_common
.globl exception_39
exception_39:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $39
    jmp exception_common
.globl exception_40
exception_40:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $40
    jmp exception_common
.globl exception_41
exception_41:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $41
    jmp exception_common
.globl exception_42
exception_42:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $42
    jmp exception_common
.globl exception_43
exception_43:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $43
    jmp exception_common
.globl exception_44
exception_44:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $44
    jmp exception_common
.globl exception_45
exception_45:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $45
    jmp exception_common
.globl exception_46
exception_46:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $46
    jmp exception_common
.globl exception_47
exception_47:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $47
    jmp exception_common
.globl exception_48
exception_48:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $48
    jmp exception_common
.globl exception_49
exception_49:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $49
    jmp exception_common
.globl exception_50
exception_50:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $50
    jmp exception_common
.globl exception_51
exception_51:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $51
    jmp exception_common
.globl exception_52
exception_52:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $52
    jmp exception_common
.globl exception_53
exception_53:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $53
    jmp exception_common
.globl exception_54
exception_54:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $54
    jmp exception_common
.globl exception_55
exception_55:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $55
    jmp exception_common
.globl exception_56
exception_56:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $56
    jmp exception_common
.globl exception_57
exception_57:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $57
    jmp exception_common
.globl exception_58
exception_58:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $58
    jmp exception_common
.globl exception_59
exception_59:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $59
    jmp exception_common
.globl exception_60
exception_60:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $60
    jmp exception_common
.globl exception_61
exception_61:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $61
    jmp exception_common
.globl exception_62
exception_62:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $62
    jmp exception_common
.globl exception_63
exception_63:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $63
    jmp exception_common
.globl exception_64
exception_64:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $64
    jmp exception_common
.globl exception_65
exception_65:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $65
    jmp exception_common
.globl exception_66
exception_66:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $66
    jmp exception_common
.globl exception_67
exception_67:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $67
    jmp exception_common
.globl exception_68
exception_68:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $68
    jmp exception_common
.globl exception_69
exception_69:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $69
    jmp exception_common
.globl exception_70
exception_70:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $70
    jmp exception_common
.globl exception_71
exception_71:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $71
    jmp exception_common
.globl exception_72
exception_72:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $72
    jmp exception_common
.globl exception_73
exception_73:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $73
    jmp exception_common
.globl exception_74
exception_74:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $74
    jmp exception_common
.globl exception_75
exception_75:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $75
    jmp exception_common
.globl exception_76
exception_76:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $76
    jmp exception_common
.globl exception_77
exception_77:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $77
    jmp exception_common
.globl exception_78
exception_78:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $78
    jmp exception_common
.globl exception_79
exception_79:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $79
    jmp exception_common
.globl exception_80
exception_80:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $80
    jmp exception_common
.globl exception_81
exception_81:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $81
    jmp exception_common
.globl exception_82
exception_82:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $82
    jmp exception_common
.globl exception_83
exception_83:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $83
    jmp exception_common
.globl exception_84
exception_84:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $84
    jmp exception_common
.globl exception_85
exception_85:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $85
    jmp exception_common
.globl exception_86
exception_86:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $86
    jmp exception_common
.globl exception_87
exception_87:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $87
    jmp exception_common
.globl exception_88
exception_88:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $88
    jmp exception_common
.globl exception_89
exception_89:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $89
    jmp exception_common
.globl exception_90
exception_90:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $90
    jmp exception_common
.globl exception_91
exception_91:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $91
    jmp exception_common
.globl exception_92
exception_92:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $92
    jmp exception_common
.globl exception_93
exception_93:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $93
    jmp exception_common
.globl exception_94
exception_94:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $94
    jmp exception_common
.globl exception_95
exception_95:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $95
    jmp exception_common
.globl exception_96
exception_96:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $96
    jmp exception_common
.globl exception_97
exception_97:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $97
    jmp exception_common
.globl exception_98
exception_98:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $98
    jmp exception_common
.globl exception_99
exception_99:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $99
    jmp exception_common
.globl exception_100
exception_100:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $100
    jmp exception_common
.globl exception_101
exception_101:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $101
    jmp exception_common
.globl exception_102
exception_102:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $102
    jmp exception_common
.globl exception_103
exception_103:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $103
    jmp exception_common
.globl exception_104
exception_104:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $104
    jmp exception_common
.globl exception_105
exception_105:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $105
    jmp exception_common
.globl exception_106
exception_106:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $106
    jmp exception_common
.globl exception_107
exception_107:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $107
    jmp exception_common
.globl exception_108
exception_108:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $108
    jmp exception_common
.globl exception_109
exception_109:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $109
    jmp exception_common
.globl exception_110
exception_110:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $110
    jmp exception_common
.globl exception_111
exception_111:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $111
    jmp exception_common
.globl exception_112
exception_112:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $112
    jmp exception_common
.globl exception_113
exception_113:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $113
    jmp exception_common
.globl exception_114
exception_114:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $114
    jmp exception_common
.globl exception_115
exception_115:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $115
    jmp exception_common
.globl exception_116
exception_116:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $116
    jmp exception_common
.globl exception_117
exception_117:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $117
    jmp exception_common
.globl exception_118
exception_118:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $118
    jmp exception_common
.globl exception_119
exception_119:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $119
    jmp exception_common
.globl exception_120
exception_120:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $120
    jmp exception_common
.globl exception_121
exception_121:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $121
    jmp exception_common
.globl exception_122
exception_122:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $122
    jmp exception_common
.globl exception_123
exception_123:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $123
    jmp exception_common
.globl exception_124
exception_124:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $124
    jmp exception_common
.globl exception_125
exception_125:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $125
    jmp exception_common
.globl exception_126
exception_126:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $126
    jmp exception_common
.globl exception_127
exception_127:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $127
    jmp exception_common
.globl exception_128
exception_128:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $128
    jmp exception_common
.globl exception_129
exception_129:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $129
    jmp exception_common
.globl exception_130
exception_130:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $130
    jmp exception_common
.globl exception_131
exception_131:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $131
    jmp exception_common
.globl exception_132
exception_132:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $132
    jmp exception_common
.globl exception_133
exception_133:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $133
    jmp exception_common
.globl exception_134
exception_134:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $134
    jmp exception_common
.globl exception_135
exception_135:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $135
    jmp exception_common
.globl exception_136
exception_136:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $136
    jmp exception_common
.globl exception_137
exception_137:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $137
    jmp exception_common
.globl exception_138
exception_138:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $138
    jmp exception_common
.globl exception_139
exception_139:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $139
    jmp exception_common
.globl exception_140
exception_140:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $140
    jmp exception_common
.globl exception_141
exception_141:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $141
    jmp exception_common
.globl exception_142
exception_142:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $142
    jmp exception_common
.globl exception_143
exception_143:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $143
    jmp exception_common
.globl exception_144
exception_144:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $144
    jmp exception_common
.globl exception_145
exception_145:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $145
    jmp exception_common
.globl exception_146
exception_146:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $146
    jmp exception_common
.globl exception_147
exception_147:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $147
    jmp exception_common
.globl exception_148
exception_148:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $148
    jmp exception_common
.globl exception_149
exception_149:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $149
    jmp exception_common
.globl exception_150
exception_150:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $150
    jmp exception_common
.globl exception_151
exception_151:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $151
    jmp exception_common
.globl exception_152
exception_152:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $152
    jmp exception_common
.globl exception_153
exception_153:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $153
    jmp exception_common
.globl exception_154
exception_154:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $154
    jmp exception_common
.globl exception_155
exception_155:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $155
    jmp exception_common
.globl exception_156
exception_156:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $156
    jmp exception_common
.globl exception_157
exception_157:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $157
    jmp exception_common
.globl exception_158
exception_158:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $158
    jmp exception_common
.globl exception_159
exception_159:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $159
    jmp exception_common
.globl exception_160
exception_160:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $160
    jmp exception_common
.globl exception_161
exception_161:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $161
    jmp exception_common
.globl exception_162
exception_162:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $162
    jmp exception_common
.globl exception_163
exception_163:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $163
    jmp exception_common
.globl exception_164
exception_164:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $164
    jmp exception_common
.globl exception_165
exception_165:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $165
    jmp exception_common
.globl exception_166
exception_166:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $166
    jmp exception_common
.globl exception_167
exception_167:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $167
    jmp exception_common
.globl exception_168
exception_168:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $168
    jmp exception_common
.globl exception_169
exception_169:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $169
    jmp exception_common
.globl exception_170
exception_170:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $170
    jmp exception_common
.globl exception_171
exception_171:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $171
    jmp exception_common
.globl exception_172
exception_172:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $172
    jmp exception_common
.globl exception_173
exception_173:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $173
    jmp exception_common
.globl exception_174
exception_174:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $174
    jmp exception_common
.globl exception_175
exception_175:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $175
    jmp exception_common
.globl exception_176
exception_176:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $176
    jmp exception_common
.globl exception_177
exception_177:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $177
    jmp exception_common
.globl exception_178
exception_178:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $178
    jmp exception_common
.globl exception_179
exception_179:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $179
    jmp exception_common
.globl exception_180
exception_180:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $180
    jmp exception_common
.globl exception_181
exception_181:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $181
    jmp exception_common
.globl exception_182
exception_182:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $182
    jmp exception_common
.globl exception_183
exception_183:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $183
    jmp exception_common
.globl exception_184
exception_184:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $184
    jmp exception_common
.globl exception_185
exception_185:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $185
    jmp exception_common
.globl exception_186
exception_186:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $186
    jmp exception_common
.globl exception_187
exception_187:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $187
    jmp exception_common
.globl exception_188
exception_188:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $188
    jmp exception_common
.globl exception_189
exception_189:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $189
    jmp exception_common
.globl exception_190
exception_190:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $190
    jmp exception_common
.globl exception_191
exception_191:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $191
    jmp exception_common
.globl exception_192
exception_192:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $192
    jmp exception_common
.globl exception_193
exception_193:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $193
    jmp exception_common
.globl exception_194
exception_194:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $194
    jmp exception_common
.globl exception_195
exception_195:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $195
    jmp exception_common
.globl exception_196
exception_196:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $196
    jmp exception_common
.globl exception_197
exception_197:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $197
    jmp exception_common
.globl exception_198
exception_198:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $198
    jmp exception_common
.globl exception_199
exception_199:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $199
    jmp exception_common
.globl exception_200
exception_200:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $200
    jmp exception_common
.globl exception_201
exception_201:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $201
    jmp exception_common
.globl exception_202
exception_202:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $202
    jmp exception_common
.globl exception_203
exception_203:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $203
    jmp exception_common
.globl exception_204
exception_204:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $204
    jmp exception_common
.globl exception_205
exception_205:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $205
    jmp exception_common
.globl exception_206
exception_206:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $206
    jmp exception_common
.globl exception_207
exception_207:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $207
    jmp exception_common
.globl exception_208
exception_208:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $208
    jmp exception_common
.globl exception_209
exception_209:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $209
    jmp exception_common
.globl exception_210
exception_210:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $210
    jmp exception_common
.globl exception_211
exception_211:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $211
    jmp exception_common
.globl exception_212
exception_212:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $212
    jmp exception_common
.globl exception_213
exception_213:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $213
    jmp exception_common
.globl exception_214
exception_214:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $214
    jmp exception_common
.globl exception_215
exception_215:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $215
    jmp exception_common
.globl exception_216
exception_216:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $216
    jmp exception_common
.globl exception_217
exception_217:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $217
    jmp exception_common
.globl exception_218
exception_218:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $218
    jmp exception_common
.globl exception_219
exception_219:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $219
    jmp exception_common
.globl exception_220
exception_220:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $220
    jmp exception_common
.globl exception_221
exception_221:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $221
    jmp exception_common
.globl exception_222
exception_222:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $222
    jmp exception_common
.globl exception_223
exception_223:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $223
    jmp exception_common
.globl exception_224
exception_224:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $224
    jmp exception_common
.globl exception_225
exception_225:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $225
    jmp exception_common
.globl exception_226
exception_226:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $226
    jmp exception_common
.globl exception_227
exception_227:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $227
    jmp exception_common
.globl exception_228
exception_228:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $228
    jmp exception_common
.globl exception_229
exception_229:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $229
    jmp exception_common
.globl exception_230
exception_230:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $230
    jmp exception_common
.globl exception_231
exception_231:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $231
    jmp exception_common
.globl exception_232
exception_232:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $232
    jmp exception_common
.globl exception_233
exception_233:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $233
    jmp exception_common
.globl exception_234
exception_234:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $234
    jmp exception_common
.globl exception_235
exception_235:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $235
    jmp exception_common
.globl exception_236
exception_236:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $236
    jmp exception_common
.globl exception_237
exception_237:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $237
    jmp exception_common
.globl exception_238
exception_238:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $238
    jmp exception_common
.globl exception_239
exception_239:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $239
    jmp exception_common
.globl exception_240
exception_240:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $240
    jmp exception_common
.globl exception_241
exception_241:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $241
    jmp exception_common
.globl exception_242
exception_242:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $242
    jmp exception_common
.globl exception_243
exception_243:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $243
    jmp exception_common
.globl exception_244
exception_244:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $244
    jmp exception_common
.globl exception_245
exception_245:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $245
    jmp exception_common
.globl exception_246
exception_246:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $246
    jmp exception_common
.globl exception_247
exception_247:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $247
    jmp exception_common
.globl exception_248
exception_248:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $248
    jmp exception_common
.globl exception_249
exception_249:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $249
    jmp exception_common
.globl exception_250
exception_250:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $250
    jmp exception_common
.globl exception_251
exception_251:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $251
    jmp exception_common
.globl exception_252
exception_252:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $252
    jmp exception_common
.globl exception_253
exception_253:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $253
    jmp exception_common
.globl exception_254
exception_254:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $254
    jmp exception_common
.globl exception_255
exception_255:
    cmpb $0, exception_entered(%rip)
    jne exception_stop
    movb $1, exception_entered(%rip)
    pushq $0
    pushq $255
    jmp exception_common
exception_common:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp,%rdi
    andq $-16,%rsp
    call exception_panic
2:  cli
    hlt
    jmp 2b
# NMI during an existing fatal exception: best-effort COM1 marker only.
# No stack use, calls, framebuffer, or dependency on C console state. Hardware
# NMI blocking remains active (no IRET); dedicated scratch slots cannot reenter
# through another naturally delivered NMI. Abort the whole marker on timeout.
nmi_emergency_stop:
    cli
    movq %rax, nmi_marker_rax(%rip)
    movq %rcx, nmi_marker_rcx(%rip)
    movq %rdx, nmi_marker_rdx(%rip)
    movq %rsi, nmi_marker_rsi(%rip)
    leaq nmi_marker(%rip), %rsi
.Lnmi_next:
    cmpb $0, (%rsi)
    je .Lnmi_done
    movl $1024, %ecx
    movw $0x3fd, %dx
.Lnmi_poll:
    inb %dx, %al
    cmpb $0xff, %al
    je .Lnmi_done
    testb $0x20, %al
    jnz .Lnmi_write
    decl %ecx
    jnz .Lnmi_poll
    jmp .Lnmi_done
.Lnmi_write:
    movw $0x3f8, %dx
    movb (%rsi), %al
    outb %al, %dx
    incq %rsi
    jmp .Lnmi_next
.Lnmi_done:
    movq nmi_marker_rax(%rip), %rax
    movq nmi_marker_rcx(%rip), %rcx
    movq nmi_marker_rdx(%rip), %rdx
    movq nmi_marker_rsi(%rip), %rsi
    jmp exception_stop
# Fatal path: never IRET, so NMI blocking remains asserted after an NMI.
.globl exception_stop
exception_stop:
    cli
    hlt
    jmp exception_stop
.data
.globl exception_stubs
exception_stubs:
    .quad exception_0
    .quad exception_1
    .quad exception_2
    .quad exception_3
    .quad exception_4
    .quad exception_5
    .quad exception_6
    .quad exception_7
    .quad exception_8
    .quad exception_9
    .quad exception_10
    .quad exception_11
    .quad exception_12
    .quad exception_13
    .quad exception_14
    .quad exception_15
    .quad exception_16
    .quad exception_17
    .quad exception_18
    .quad exception_19
    .quad exception_20
    .quad exception_21
    .quad exception_22
    .quad exception_23
    .quad exception_24
    .quad exception_25
    .quad exception_26
    .quad exception_27
    .quad exception_28
    .quad exception_29
    .quad exception_30
    .quad exception_31
    .quad exception_32
    .quad exception_33
    .quad exception_34
    .quad exception_35
    .quad exception_36
    .quad exception_37
    .quad exception_38
    .quad exception_39
    .quad exception_40
    .quad exception_41
    .quad exception_42
    .quad exception_43
    .quad exception_44
    .quad exception_45
    .quad exception_46
    .quad exception_47
    .quad exception_48
    .quad exception_49
    .quad exception_50
    .quad exception_51
    .quad exception_52
    .quad exception_53
    .quad exception_54
    .quad exception_55
    .quad exception_56
    .quad exception_57
    .quad exception_58
    .quad exception_59
    .quad exception_60
    .quad exception_61
    .quad exception_62
    .quad exception_63
    .quad exception_64
    .quad exception_65
    .quad exception_66
    .quad exception_67
    .quad exception_68
    .quad exception_69
    .quad exception_70
    .quad exception_71
    .quad exception_72
    .quad exception_73
    .quad exception_74
    .quad exception_75
    .quad exception_76
    .quad exception_77
    .quad exception_78
    .quad exception_79
    .quad exception_80
    .quad exception_81
    .quad exception_82
    .quad exception_83
    .quad exception_84
    .quad exception_85
    .quad exception_86
    .quad exception_87
    .quad exception_88
    .quad exception_89
    .quad exception_90
    .quad exception_91
    .quad exception_92
    .quad exception_93
    .quad exception_94
    .quad exception_95
    .quad exception_96
    .quad exception_97
    .quad exception_98
    .quad exception_99
    .quad exception_100
    .quad exception_101
    .quad exception_102
    .quad exception_103
    .quad exception_104
    .quad exception_105
    .quad exception_106
    .quad exception_107
    .quad exception_108
    .quad exception_109
    .quad exception_110
    .quad exception_111
    .quad exception_112
    .quad exception_113
    .quad exception_114
    .quad exception_115
    .quad exception_116
    .quad exception_117
    .quad exception_118
    .quad exception_119
    .quad exception_120
    .quad exception_121
    .quad exception_122
    .quad exception_123
    .quad exception_124
    .quad exception_125
    .quad exception_126
    .quad exception_127
    .quad exception_128
    .quad exception_129
    .quad exception_130
    .quad exception_131
    .quad exception_132
    .quad exception_133
    .quad exception_134
    .quad exception_135
    .quad exception_136
    .quad exception_137
    .quad exception_138
    .quad exception_139
    .quad exception_140
    .quad exception_141
    .quad exception_142
    .quad exception_143
    .quad exception_144
    .quad exception_145
    .quad exception_146
    .quad exception_147
    .quad exception_148
    .quad exception_149
    .quad exception_150
    .quad exception_151
    .quad exception_152
    .quad exception_153
    .quad exception_154
    .quad exception_155
    .quad exception_156
    .quad exception_157
    .quad exception_158
    .quad exception_159
    .quad exception_160
    .quad exception_161
    .quad exception_162
    .quad exception_163
    .quad exception_164
    .quad exception_165
    .quad exception_166
    .quad exception_167
    .quad exception_168
    .quad exception_169
    .quad exception_170
    .quad exception_171
    .quad exception_172
    .quad exception_173
    .quad exception_174
    .quad exception_175
    .quad exception_176
    .quad exception_177
    .quad exception_178
    .quad exception_179
    .quad exception_180
    .quad exception_181
    .quad exception_182
    .quad exception_183
    .quad exception_184
    .quad exception_185
    .quad exception_186
    .quad exception_187
    .quad exception_188
    .quad exception_189
    .quad exception_190
    .quad exception_191
    .quad exception_192
    .quad exception_193
    .quad exception_194
    .quad exception_195
    .quad exception_196
    .quad exception_197
    .quad exception_198
    .quad exception_199
    .quad exception_200
    .quad exception_201
    .quad exception_202
    .quad exception_203
    .quad exception_204
    .quad exception_205
    .quad exception_206
    .quad exception_207
    .quad exception_208
    .quad exception_209
    .quad exception_210
    .quad exception_211
    .quad exception_212
    .quad exception_213
    .quad exception_214
    .quad exception_215
    .quad exception_216
    .quad exception_217
    .quad exception_218
    .quad exception_219
    .quad exception_220
    .quad exception_221
    .quad exception_222
    .quad exception_223
    .quad exception_224
    .quad exception_225
    .quad exception_226
    .quad exception_227
    .quad exception_228
    .quad exception_229
    .quad exception_230
    .quad exception_231
    .quad exception_232
    .quad exception_233
    .quad exception_234
    .quad exception_235
    .quad exception_236
    .quad exception_237
    .quad exception_238
    .quad exception_239
    .quad exception_240
    .quad exception_241
    .quad exception_242
    .quad exception_243
    .quad exception_244
    .quad exception_245
    .quad exception_246
    .quad exception_247
    .quad exception_248
    .quad exception_249
    .quad exception_250
    .quad exception_251
    .quad exception_252
    .quad exception_253
    .quad exception_254
    .quad exception_255
.bss
.balign 16
.section .rodata
nmi_marker: .asciz "\nPANIC.SECOND_FAULT\n"
.balign 8
.bss
nmi_marker_rax: .quad 0
nmi_marker_rcx: .quad 0
nmi_marker_rdx: .quad 0
nmi_marker_rsi: .quad 0
nmi_saved_rax: .quad 0
nmi_entered: .byte 0
.globl exception_entered
exception_entered: .byte 0
.balign 16
nmi_spare: .skip 16384
nmi_spare_top:
.section .note.GNU-stack,"",@progbits
