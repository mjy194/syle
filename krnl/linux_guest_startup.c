#include "linux_guest_dispatch.h"
#include "linux_guest_manager.h"
#ifndef MG_FIXED_INPUTS_VERIFIED
#error "Managed guest requires pinned-image hash gate before compiling startup"
#endif
#if (defined(MG_LEGACY_PIC)+defined(MG_LEGACY_PIC04)+defined(MG_EXPERIMENTAL_APIC)) > 1
#error "Legacy PIC and experimental APIC profiles are mutually exclusive"
#endif
#ifdef MG_EXPERIMENTAL_APIC
#include "linux_profile_service.h"
static struct lps_context profile;
#else
static struct lgd_context dispatcher;
#endif
static struct lgm_service_ops service;
/* Explicit ownership of logical UART level only. NOT a LAPIC injection sink.
 * Guest CPUID hides APIC; this profile is polled UART until a routed IRQ backend
 * is separately installed and verified. Do not label true as guest delivery. */
static bool uart_irq4_level;
static bool retain_level(void *ctx,unsigned irq,bool level) {
 (void)ctx;if(irq!=4)return false;uart_irq4_level=level;return true;
}
static void cpu(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
bool linux_guest_startup_install(void) {
 struct lgd_config c={.max_exits=1u<<20,.fixed_input_verified=true,.sync_irq=retain_level};
 #ifdef MG_LEGACY_PIC
 c.legacy_pic_enabled=true;c.legacy_idle_advance=true;
 #endif
 #ifdef MG_LEGACY_PIC04
 c.legacy_pic04_enabled=true;c.legacy_idle_advance=true;c.sync_irq=0;c.irq_ctx=0;
 #endif
 uint32_t a,b,x,d;
 cpu(0,&c.max_basic,&c.vendor_b,&c.vendor_c,&c.vendor_d);
 cpu(0x80000000,&c.max_extended,&b,&x,&d);
 if(c.max_basic<1||c.max_extended<0x80000008)return false;
 cpu(1,&a,&b,&x,&c.cpu.basic_edx);
 cpu(0x80000001,&a,&b,&x,&c.cpu.extended_edx);
 cpu(0x80000008,&a,&b,&x,&d);c.cpu.physical_bits=a&255;
#ifdef MG_EXPERIMENTAL_APIC
 return lps_init(&profile,&c,&profile.dispatch,lgd_apic_now,&service)&&linux_guest_manager_install(&service);
#else
 return lgd_init(&dispatcher,&c,&service)&&linux_guest_manager_install(&service);
#endif
}

#ifdef MG_PROGRESS_DIAG
#if (!defined(MG_LEGACY_PIC)&&!defined(MG_LEGACY_PIC04)) || defined(MG_EXPERIMENTAL_APIC)
#error "Progress diagnostic is limited to opt-in legacy PIC target"
#endif
#include "linux_progress_diag.h"
#include "../efi/src/exceptions.h"
static struct lprog_gate progress_gate;
static unsigned progress_pending;
static bool progress_irq4_sent;
void linux_guest_progress_note(struct lgs_session*s){
 if(!s||s->state.host_dirty||!s->state.ready||!s->platform.safe||!s->platform.safe(s->platform.ctx))return;
 if(dispatcher.config.legacy_pic04_enabled&&dispatcher.pic04.master.reserved.serial)progress_pending|=2;
 if(dispatcher.config.legacy_pic04_enabled&&dispatcher.pic04.master.completed.serial){progress_pending|=4;if(dispatcher.pic04.master.completed.source==4)progress_pending|=8;}
 if(dispatcher.legacy_initialized&&dispatcher.legacy.timer.pic.reserved)progress_pending|=2;
 if(dispatcher.legacy_initialized&&dispatcher.legacy.timer.pic.completed)progress_pending|=4;
}
void linux_guest_progress_probe(struct lgs_session*s,bool after,bool success){
 struct lprog_snapshot v;
 if(!s)return;
 unsigned milestones=progress_pending;
 if(dispatcher.config.legacy_pic04_enabled&&dispatcher.pic04.master.reserved.serial)milestones|=2;
 if(dispatcher.config.legacy_pic04_enabled&&dispatcher.pic04.master.completed.serial)milestones|=4;
 if(after&&success&&s->last_reason==12)milestones|=1;
 if(dispatcher.legacy_initialized&&dispatcher.legacy.timer.pic.reserved)milestones|=2;
 if(dispatcher.legacy_initialized&&dispatcher.legacy.timer.pic.completed)milestones|=4;
 bool due=lprog_due(&progress_gate,s->exits,after,success);
 bool marked=lprog_mark(&progress_gate,milestones);
 bool irq4=(progress_pending&8)&&!progress_irq4_sent;
 if((!due&&!marked&&!irq4)||!lprog_collect(&dispatcher,s,&v))return;
 if(irq4)(void)lprog_irq4_once(&progress_irq4_sent,true);
 log_text(after?"MANAGED.PROGRESS after":"MANAGED.PROGRESS before");
 #define P(label,field) do{log_text(label);log_hex(v.field);}while(0)
 P(" irq_source=",irq_source);P(" receipt_source=",receipt_source);P(" uart_level=",uart_level);P(" uart_dirty=",uart_dirty);P(" uart_edges=",uart_edges);P(" idle_reason=",idle_reason);
 P(" exits=",exits);P(" ticks=",ticks);P(" reload=",reload);P(" count=",count);P(" edges=",edges);P(" irr=",irr);P(" isr=",isr);P(" token=",reserved);P(" lei=",phase);P(" raw_reason=",reason);P(" raw_rip=",rip);P(" stage=",stage);P(" result=",result);
 log_text(" milestones=");log_hex(milestones);log_text(" timer_valid=");log_hex(v.timer_valid);log_text(" vmcs_valid=");log_hex(v.vmcs_valid);log_text("\n");
 #undef P
}
#endif
