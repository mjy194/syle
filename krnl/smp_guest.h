#ifndef SYLE_SMP_GUEST_H
#define SYLE_SMP_GUEST_H
#include <stdbool.h>
#include <stdint.h>
struct smp_guest_frame {
    uint64_t rax,rcx,rdx,rbx,rbp,rsi,rdi,r8,r9,r10,r11,r12,r13,r14,r15;
    uint64_t guest_cr2,host_cr2;
};
struct smp_guest_table { uint16_t limit; uint64_t base; } __attribute__((packed));
struct smp_guest_host {
    struct smp_guest_table gdtr,idtr;
    uint64_t cr0,cr3,cr4,cr2,fs_base,gs_base,pat,efer;
    uint64_t sysenter_cs,sysenter_esp,sysenter_eip;
    uint16_t selectors[8];
};
enum smp_guest_status {
    SMP_GUEST_OK, SMP_GUEST_BAD_CONTEXT, SMP_GUEST_WRONG_CPU,
    SMP_GUEST_BLOCKED, SMP_GUEST_CLEAR_FAILED, SMP_GUEST_LOAD_FAILED,
    SMP_GUEST_SETUP_FAILED, SMP_GUEST_FLUSH_FAILED, SMP_GUEST_ENTER_FAILED,
    SMP_GUEST_EXIT_MISMATCH, SMP_GUEST_HOST_MISMATCH
};
struct smp_guest_context {
    uint64_t vmcs_hpa;
    void *vmcs_view;
    uint64_t eptp,stack_top,expected_data,owner_token,vmcall_gpa;
    uint32_t owner_apic_id;
    struct smp_guest_frame frame;
    struct smp_guest_host host;
    uint64_t exit_reason,exit_rip,exit_length,exit_rsp,instruction_error;
    uint64_t exit_info,exit_flags,loop_start,loop_end;
    bool kick_mode,kicked,cache_guard;
    enum smp_guest_status status;
    unsigned enter_result;
    bool vmcs_live,cleared,host_restored,launched;
};
static inline bool smp_guest_exit_valid(const struct smp_guest_context *c) {
    return c&&c->host_restored&&c->exit_reason==18&&c->exit_rip==c->vmcall_gpa&&
        c->exit_length==3&&c->exit_rsp==c->stack_top&&c->frame.rax==c->expected_data&&
        c->frame.r8==c->owner_token&&c->frame.r9==c->owner_token;
}
static inline bool smp_guest_resume_valid(const struct smp_guest_context *c) {
    return c&&c->vmcs_live&&!c->cleared&&c->launched&&c->status==SMP_GUEST_OK&&
        c->vmcall_gpa>=0x6000&&c->vmcall_gpa<=0x6ffd&&smp_guest_exit_valid(c);
}
static inline bool smp_guest_kick_valid(const struct smp_guest_context *c) {
    return c&&c->host_restored&&c->kick_mode&&c->exit_reason==1&&
        c->exit_info==UINT64_C(0x80000041)&&!(c->exit_flags&(1u<<9))&&
        c->loop_start>=0x6000&&c->loop_end<=0x7000&&c->loop_start<c->loop_end&&
        c->exit_rip>=c->loop_start&&c->exit_rip<c->loop_end&&
        c->exit_rsp==c->stack_top&&c->frame.r8==c->owner_token&&c->frame.r9==c->owner_token;
}
/* Caller owns unique WB VMCS page and already-entered local VMX root.
 * Shared fixture: CR3=1000/GDT=5000/TR=5100/IDT=5800/code=6000/data=8000.
 * stack_top must be a private mapped guest stack top (a000 or b000).
 * All shared backing is preallocated, immutable while either CPU runs.
 * IF=0, no allocation/logging. Failed clear retains vmcs_live: do not free.
 * This is a bounded trusted guest, not a general VM isolation boundary. */
bool smp_guest_local_run(struct smp_guest_context *ctx);
bool smp_guest_local_warm(struct smp_guest_context *ctx);
bool smp_guest_local_resume(struct smp_guest_context *ctx,uint64_t expected);
bool smp_guest_local_invalidate(struct smp_guest_context *ctx);
bool smp_guest_local_clear(struct smp_guest_context *ctx);
int smp_guest_enter(struct smp_guest_frame *frame,unsigned resume);
extern const unsigned char smp_guest_payload_start[],smp_guest_payload_end[],smp_guest_payload_call[];
extern const uint32_t smp_guest_payload_call_offset;
#endif
