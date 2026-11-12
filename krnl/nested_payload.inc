.global ept_payload_start,ept_payload_end,ept_normal_call,ept_read_fault,ept_write_fault,ept_done_call
.global nested_l1_exit,nested_l2_entry,nested_l2_resume
/* Actual L1 VMX instructions, copied PIC into private guest code. */
ept_payload_start:
    cli
    mov $1,%eax
    cpuid
    test $0x20,%ecx
    jz .Lnested_fail
    mov $0x3a,%ecx
    rdmsr
    cmp $5,%eax
    jne .Lnested_fail
#if defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
    /* L0 audited immutable source tables. L1 only constructs its VMCS12. */
#elif defined(VMX_NESTED_UC_PAT_TEST)
#include "nested_uc_pat_init.inc"
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
#include "nested_uc_context_init.inc"
#elif defined(VMX_NESTED_UC_TEST)
#include "nested_uc_init.inc"
#elif defined(VMX_NESTED_MEMORY_TEST)
#include "nested_memory_init.inc"
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
#include "nested_ept_ad_mode_init.inc"
#else
#include "nested_ept_ad_context_init.inc"
#endif
#elif defined(VMX_NESTED_EPT_AD_FAIL_TEST)
#include "nested_ept_ad_fail_init.inc"
#elif defined(VMX_NESTED_EPT_AD_TEST)
#include "nested_ept_ad_init.inc"
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
#include "nested_ept_misconfig_init.inc"
#elif defined(VMX_NESTED_EPT_LARGE_TEST)
#include "nested_ept_large_init.inc"
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
#include "nested_ept_dynamic_init.inc"
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
#include "nested_ept_context_init.inc"
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
#include "nested_ept_sparse_init.inc"
#elif defined(VMX_NESTED_EPT_MAP_TEST)
#include "nested_ept_map_init.inc"
#elif defined(VMX_NESTED_EPT_TEST)
    /* L1 really builds its EPT12 tables in its own guest-physical RAM. */
    mov $0x10000,%ebx
    movq $0x11007,(%rbx)
    mov $0x11000,%ebx
    movq $0x12007,(%rbx)
    mov $0x12000,%ebx
    movq $0x13007,(%rbx)
    mov $0x13000,%ebx
    mov $1,%ecx
.Lnested_ept_leaves:
    mov %rcx,%rax
    shl $12,%rax
    or $0x33,%rax
    mov %rax,(%rbx,%rcx,8)
    inc %ecx
    cmp $16,%ecx
    jne .Lnested_ept_leaves
    movq $0x6035,48(%rbx) /* code RX */
    movq $0x8031,120(%rbx) /* L2 f000 aliases L1 8000, initially RO */
#endif
    mov $0xc810,%eax
    vmxon (%rax)
    jbe .Lnested_fail
#ifdef VMX_NESTED_FAILURE_TEST
    mov $0x681e,%ecx
    mov $0x1234,%eax
    vmread %rcx,%rax /* no current VMCS: VMfailInvalid, dest unchanged */
    jnc .Lnested_fail
    jz .Lnested_fail
    cmp $0x1234,%eax
    jne .Lnested_fail
#endif
    mov $0xc818,%eax
    vmclear (%rax)
    jbe .Lnested_fail
    vmptrld (%rax)
    jbe .Lnested_fail
    mov $0xc000,%esi
    mov $0xc800,%eax
    mov (%rax),%rdi
.Lnested_fields:
    mov (%rsi),%rcx
    mov 8(%rsi),%rdx
    vmwrite %rdx,%rcx
    jbe .Lnested_fail
    add $16,%rsi
    dec %rdi
    jnz .Lnested_fields
#ifdef VMX_NESTED_FAILURE_TEST
#include "nested_failure_attempt.inc"
#else
#ifdef VMX_NESTED_PF_INJECT_TEST
    mov $0x11110000,%eax
    mov %rax,%cr2
#endif
    vmlaunch
#endif
    jmp .Lnested_fail /* successful launch never falls through */
nested_l1_exit:
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
#include "nested_source_replace_l1.inc"
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
#include "nested_atomic_multiround_l1.inc"
#elif defined(VMX_NESTED_ATOMIC_RETRACK_TEST)
#include "nested_atomic_retrack_l1.inc"
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#include "nested_atomic_catalog_l1.inc"
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
#include "nested_typed_dynamic_l1.inc"
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%eax
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%eax
    jne .Lnested_fail
    incq 0xc820
    cmpq $6,0xc820
    je .Lnested_done
    ja .Lnested_fail
    mov 0xc820,%rax
    and $1,%eax
    shl $14,%rax
    add $0x1001e,%rax
    mov $0x201a,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
#elif defined(VMX_NESTED_OWNER_TYPED_TEST)
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%eax
    jne .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $3,%eax
    jne .Lnested_fail
    incq 0xc820
    cmpq $3,0xc820
    je .Lnested_done
    cmpq $1,0xc820
    je .Ltyped_select_uc
    cmpq $2,0xc820
    jne .Lnested_fail
    lea .Ltyped_wb_entry(%rip),%rax
    jmp .Ltyped_resume
.Ltyped_select_uc:
    lea .Ltyped_uc_entry(%rip),%rax
.Ltyped_resume:
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
#elif defined(VMX_NESTED_OWNER_AD_RESET_TEST)
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $18,%eax
    jne .Lnested_fail
    incq 0xc820
    cmpq $3,0xc820
    je .Lnested_done
    cmpq $2,0xc820
    ja .Lnested_fail
    lea nested_l2_resume(%rip),%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
#elif defined(VMX_NESTED_OWNER_CONTEXT_TEST)
#include "nested_owner_context_l1.inc"
#elif defined(VMX_NESTED_UC_PAT_TEST)
#include "nested_uc_pat_l1.inc"
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
#include "nested_uc_context_l1.inc"
#elif defined(VMX_NESTED_UC_TEST)
#include "nested_uc_l1.inc"
#elif defined(VMX_NESTED_MEMORY_TEST)
#include "nested_memory_l1.inc"
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
#include "nested_ept_ad_mode_l1.inc"
#else
#include "nested_ept_ad_context_l1.inc"
#endif
#elif defined(VMX_NESTED_EPT_AD_FAIL_TEST)
#include "nested_ept_ad_fail_l1.inc"
#elif defined(VMX_NESTED_EPT_AD_TEST)
#include "nested_ept_ad_l1.inc"
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
#include "nested_ept_misconfig_l1.inc"
#elif defined(VMX_NESTED_EPT_LARGE_TEST)
#include "nested_ept_large_l1.inc"
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
#include "nested_ept_dynamic_l1.inc"
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
#include "nested_ept_context_l1.inc"
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
#include "nested_ept_sparse_l1.inc"
#elif defined(VMX_NESTED_EPT_MAP_TEST)
#include "nested_ept_map_l1.inc"
#elif defined(VMX_NESTED_PF_ROUTE_TEST)
#include "nested_pf_route_l1.inc"
#elif defined(VMX_NESTED_EVENT_TEST)
#include "nested_event_l1.inc"
#elif defined(VMX_NESTED_FAILURE_TEST)
#include "nested_failure_handler.inc"
#elif defined(VMX_NESTED_STATE_TEST)
#include "nested_state_l1.inc"
#else
    mov $0x4402,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
#ifdef VMX_NESTED_EPT_TEST
    cmp $18,%eax
    je .Lnested_done
    cmp $48,%eax
    jne .Lnested_fail
    mov $0x2400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    cmp $0xf000,%rax
    jne .Lnested_fail
    mov $0x6400,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    and $0x1ff,%eax
    cmp $0x18a,%eax
    jne .Lnested_fail
    mov $0x13078,%ebx
    movq $0x8033,(%rbx) /* L1 grants write permission */
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
    vmresume /* original fault RIP, no skip */
    jmp .Lnested_fail
#else
    cmp $18,%eax
    jne .Lnested_fail
    mov $0xc820,%ebx
    incq (%rbx)
    cmpq $2,(%rbx)
    je .Lnested_done
    cmpq $1,(%rbx)
    jne .Lnested_fail
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
    cmpq $0,0xc828
    jne .Llocal_not_writer
    movq $0xb033,0x13078
.Llocal_not_writer:
    movq $0x1001e,0xc830
    movq $0,0xc838
    mov $0xc830,%ebx
    mov $1,%eax
    invept (%rbx),%rax
    jbe .Lnested_fail
#endif
    mov $0x681e,%ecx
    vmread %rcx,%rax
    jbe .Lnested_fail
    mov $0x440c,%ecx
    vmread %rcx,%rdx
    jbe .Lnested_fail
    add %rdx,%rax
    mov $0x681e,%ecx
    vmwrite %rax,%rcx
    jbe .Lnested_fail
    vmresume
    jmp .Lnested_fail
#endif
#endif
.Lnested_done:
    mov $0xc818,%eax
    vmclear (%rax)
    jbe .Lnested_fail
    vmxoff
    jbe .Lnested_fail
    mov $0x4e31,%eax
    vmcall
.Lnested_fail:
    ud2
nested_l2_entry:
#if defined(VMX_NESTED_PARALLEL_TEST) && !defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST) && !defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#include "nested_parallel_l2.inc"
#endif
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
#include "nested_source_replace_l2.inc"
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
#ifdef VMX_NESTED_ATOMIC_GIGAPAGE_TEST
#include "nested_atomic_gigapage_l2.inc"
#elif defined(VMX_NESTED_ATOMIC_LARGEPAGE_TEST)
#include "nested_atomic_largepage_l2.inc"
#else
#include "nested_atomic_multiround_l2.inc"
#endif
#elif defined(VMX_NESTED_ATOMIC_RETRACK_TEST)
#include "nested_atomic_retrack_l2.inc"
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#include "nested_atomic_catalog_l2.inc"
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
#include "nested_typed_dynamic_l2.inc"
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
.global nested_typed_context_call0,nested_typed_context_call1,nested_typed_context_call2
.global nested_typed_context_call3,nested_typed_context_call4,nested_typed_context_call5
.global nested_typed_warm_call,nested_typed_uc_entry,nested_typed_uc_call
.global nested_typed_wb_entry,nested_typed_wb_call
    movabs $0x38ba38ba38ba38ba,%rax
    cmp %rax,0x30000
    jne .Lnested_fail
    pushq $2
    popfq
nested_typed_warm_call:
nested_typed_context_call0:
    vmcall
    ud2
nested_l2_resume:
nested_typed_uc_entry:
nested_typed_wb_entry:
    mov 0xc820,%r10
    cmp $1,%r10
    jb .Lnested_fail
    cmp $5,%r10
    ja .Lnested_fail
    mov %r10,%r8
    and $1,%r8
    movabs $0x38ba38ba38ba38ba,%rax
    test %r8,%r8
    jz .Ltyped_context_witness
    movabs $0x38bb38bb38bb38bb,%rax
.Ltyped_context_witness:
    cmp %rax,0x30000
    jne .Lnested_fail
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    cmp $1,%r10
    je .Ltyped_context_call1
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    lea (%rcx,%r8,2),%rdx
    movabs $0x38bc38bc38bc3800,%rax
    add %rdx,%rax
    cmp $3,%r10
    ja .Ltyped_context_restored
    mov %rax,0xf008(,%rdx,8)
    cmp %rax,0xf008(,%rdx,8)
    jne .Lnested_fail
    mov %rax,32(%r12,%r8,8)
    cmp $2,%r10
    je .Ltyped_context_call2
    jmp .Ltyped_context_call3
.Ltyped_context_restored:
    cmp %rax,0xf008(,%rdx,8)
    jne .Lnested_fail
    mov %rax,48(%r12,%r8,8)
    cmp $4,%r10
    je .Ltyped_context_call4
    jmp .Ltyped_context_call5
.Ltyped_context_call1:
    pushq $2
    popfq
nested_typed_context_call1:
    vmcall
    ud2
.Ltyped_context_call2:
    pushq $2
    popfq
nested_typed_uc_call:
nested_typed_context_call2:
    vmcall
    ud2
.Ltyped_context_call3:
    pushq $2
    popfq
nested_typed_context_call3:
    vmcall
    ud2
.Ltyped_context_call4:
    pushq $2
    popfq
nested_typed_wb_call:
nested_typed_context_call4:
    vmcall
    ud2
.Ltyped_context_call5:
    pushq $2
    popfq
nested_typed_context_call5:
    vmcall
    ud2
#elif defined(VMX_NESTED_OWNER_TYPED_TEST)
.global nested_typed_warm_call,nested_typed_uc_entry,nested_typed_uc_call
.global nested_typed_wb_entry,nested_typed_wb_call
nested_typed_warm_call:
    vmcall
    ud2
nested_l2_resume:
nested_typed_uc_entry:
.Ltyped_uc_entry:
    cmpq $1,0xc820
    jne .Lnested_fail
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    movabs $0x36bc36bc36bc3600,%rax
    add %rcx,%rax
    mov %rax,0xf008(,%rcx,8)
    cmp %rax,0xf008(,%rcx,8)
    jne .Lnested_fail
    mov %rax,32(%r12)
    pushq $2
    popfq
nested_typed_uc_call:
    vmcall
    ud2
nested_typed_wb_entry:
.Ltyped_wb_entry:
    cmpq $2,0xc820
    jne .Lnested_fail
    movabs $0x3344334433443344,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov 0xc828,%rcx
    cmp $1,%rcx
    ja .Lnested_fail
    movabs $0x36bc36bc36bc3600,%rax
    add %rcx,%rax
    cmp %rax,0xf008(,%rcx,8)
    jne .Lnested_fail
    mov %rax,40(%r12)
    pushq $2
    popfq
nested_typed_wb_call:
    vmcall
    ud2
#elif defined(VMX_NESTED_UC_PAT_TEST)
#include "nested_uc_pat_l2.inc"
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
#include "nested_uc_context_l2.inc"
#elif defined(VMX_NESTED_UC_TEST)
#include "nested_uc_l2.inc"
#elif defined(VMX_NESTED_MEMORY_TEST)
#include "nested_memory_l2.inc"
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
#include "nested_ept_ad_mode_l2.inc"
#else
#include "nested_ept_ad_context_l2.inc"
#endif
#elif defined(VMX_NESTED_EPT_AD_FAIL_TEST)
#include "nested_ept_ad_fail_l2.inc"
#elif defined(VMX_NESTED_EPT_AD_TEST)
#include "nested_ept_ad_l2.inc"
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
#include "nested_ept_misconfig_l2.inc"
#elif defined(VMX_NESTED_EPT_LARGE_TEST)
#include "nested_ept_large_l2.inc"
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
#include "nested_ept_dynamic_l2.inc"
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
#include "nested_ept_context_l2.inc"
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
#include "nested_ept_sparse_l2.inc"
#elif defined(VMX_NESTED_EPT_MAP_TEST)
#include "nested_ept_map_l2.inc"
#elif defined(VMX_NESTED_PF_ROUTE_TEST)
#include "nested_pf_route_l2.inc"
#elif defined(VMX_NESTED_EVENT_TEST)
#include "nested_event_l2.inc"
#elif defined(VMX_NESTED_STATE_TEST)
#include "nested_state_l2.inc"
#else
#ifdef VMX_NESTED_EPT_TEST
    mov $0xf000,%edi
nested_l2_resume:
    movq $0x77,(%rdi)
    vmcall
#else
    vmcall
nested_l2_resume:
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
    mov 0xf000,%rax
    movabs $0x3344334433443344,%rdx
    cmp %rdx,%rax
    jne .Lnested_fail
    cmpq $1,0xc820
    je .Lad_reset_read
    cmpq $2,0xc820
    jne .Lnested_fail
    cmpq $1,0xc828
    jne .Lad_reset_read
    movabs $0x35cc35cc35cc35cc,%rax
    mov %rax,0xf008
.Lad_reset_read:
    pushq $2
    popfq
.global nested_ad_reset_call
nested_ad_reset_call:
#endif
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
    mov 0xf000,%rax
    pushq $2
    popfq
.global nested_owner_context_call
nested_owner_context_call:
#endif
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
    movabs $0x7788778877887788,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov %rax,32(%r12)
    pushq $2
    popfq
.global nested_local_second_call
nested_local_second_call:
#endif
#ifdef VMX_NESTED_RECLAIM_TEST
    movabs $0x5566556655665566,%rax
    cmp %rax,0xf000
    jne .Lnested_fail
    mov 0xf000,%rax
    mov %rax,32(%r12)
    pushq $2
    popfq
.global nested_reclaim_second_call
nested_reclaim_second_call:
#endif
    vmcall
#endif
    ud2
#endif
/* Existing fixture/link compatibility only, never used to decode VMX. */
ept_normal_call:
ept_read_fault:
ept_write_fault:
ept_done_call:
    ud2
ept_payload_end:
.global ept_exec_start,ept_exec_end
ept_exec_start:
    ret
ept_exec_end:
