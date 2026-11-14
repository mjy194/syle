#include "../linux_guest_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
static jmp_buf fatal_jump;
static bool expect_fatal;
static struct lgs_session s;
static struct {unsigned prepares,opens,closes,frees,entries,enables,disables,writes,before,after;int override_result,close_fail,enter_fail,dispatch_fail,prepare_fail,open_fail,dirty,before_fail,after_fail,read_fail;uint64_t reason,vectoring;bool custom_fields;uint32_t expected_resume;} f;
bool lgm_prepare(struct linux_guest_memory*m,const struct sg_memory_ops*o,const void*k,size_t ks,const void*i,size_t is,const char*c){(void)o;(void)k;(void)ks;(void)i;(void)is;(void)c;assert(!m->prepared);m->prepared=true;m->eptp=0x101e;++f.prepares;return !f.prepare_fail;}
bool lgm_release(struct linux_guest_memory*m){assert(!s.host.active);m->prepared=false;++f.frees;return true;}
bool lvh_open(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip){(void)o;(void)m;(void)rsp;(void)rip;assert(s.enabled);h->active=h->ready=true;h->caps.timer_shift=4;h->plan.gpr[4]=0x90000;++f.opens;return !f.open_fail;}
bool lvh_close(struct lvh_context*h){++f.closes;if(f.close_fail)return false;h->active=h->ready=false;return true;}
bool lvs_init(struct lvs_state*v){if(v->ready)return false;v->ready=v->guest_valid=true;return true;}
enum lvs_result lvs_run(struct lvs_state*v,const struct lvs_ops*o){(void)v;(void)o;assert(s.native.resume==f.expected_resume);++f.entries;v->host_dirty=!!f.dirty;return f.override_result?(enum lvs_result)(f.override_result-1):(f.enter_fail?LVS_ENTRY_FAILED:LVS_EXIT);}
static bool enable(void*x){(void)x;++f.enables;return true;}
static bool disable(void*x){(void)x;assert(!s.host.active);++f.disables;return true;}
static bool safe(void*x){(void)x;return true;}
static bool rd(void*x,uint32_t field,uint64_t*v){(void)x;*v=field==0x4402?(f.custom_fields?f.reason:52):f.vectoring;return !f.read_fail;}
static struct lgs_entry_result receipt;
static bool before(void*x,struct lgs_session*z){(void)x;assert(z==&s);++f.before;return !f.before_fail;}
static bool after(void*x,struct lgs_session*z,const struct lgs_entry_result*r){(void)x;assert(z==&s);++f.after;receipt=*r;return !f.after_fail;}
static bool dispatch(void*x,struct lgs_session*z,uint32_t r){(void)x;assert(z==&s&&r==52);return !f.dispatch_fail;}
static void fatal(void*x){(void)x;if(expect_fatal)longjmp(fatal_jump,1);assert(!"unexpected fatal");}
static bool write(void*x,uint32_t field,uint64_t value){(void)x;assert(field==0x482e&&value==(1u<<24));++f.writes;return true;}
static bool invalidate(void*x,uint64_t eptp){(void)x;assert(eptp==0x101e);return true;}
static bool hook_fail;static unsigned hook_calls;
static bool prepare_hook(void*x,struct lgs_session*z){(void)x;hook_calls++;
 assert(z->memory.prepared&&!z->prepared&&!z->enabled&&!z->host.active&&z->prepare_stage==9);
 assert(!z->native.resume&&!z->exits&&!z->last_reason);return !hook_fail;}
int main(void){
 const char asset='x';struct lvh_ops h={.vmwrite=write,.invept=invalidate};struct lvs_ops v={0};
 struct lgs_platform p={.enable=enable,.disable=disable,.safe=safe,.read_field=rd,.dispatch=dispatch,.fatal=fatal};
 assert(lgs_configure(&s,&h,&v,&p,&asset,1,&asset,1,"",0x1000,0x1000));
 s.platform.prepare_guest=prepare_hook;hook_fail=true;s.native.resume=1;s.exits=5;s.last_reason=10;
 assert(lgs_guest_backend.prepare(&s,1)&&s.failed&&s.memory.prepared&&s.prepare_stage==9&&!f.enables&&!f.opens);
 assert(hook_calls==1);assert(lgs_guest_backend.prepare(&s,1)&&hook_calls==1);
 assert(!lgs_guest_backend.release(&s));hook_fail=false;f=(__typeof__(f)){0};
 assert(!lgs_configure(&s,&h,&v,&p,&asset,1,&asset,1,"",0x1000,0x1000));
 assert(lgs_guest_backend.prepare(&s,2));assert(!f.prepares);
 assert(!lgs_guest_backend.prepare(&s,1));assert(s.native.gpr[4]==0x90000);
 assert(!lgs_guest_backend.start(&s));f.expected_resume=1;
 assert(!lgs_guest_backend.start(&s));assert(!lgs_guest_backend.stop(&s));
 assert(!f.closes&&!f.frees);assert(!lgs_guest_backend.start(&s));assert(f.opens==1&&f.prepares==1&&f.entries==3);
 f.close_fail=1;assert(lgs_guest_backend.release(&s));assert(!f.frees&&!f.disables&&s.host.active);
 f.close_fail=0;assert(!lgs_guest_backend.release(&s));assert(f.frees==1&&f.disables==1&&!s.enabled);
 /* New create starts from a new launch, not the old VMCS/MSR state. */
 assert(!lgs_guest_backend.prepare(&s,1));f.expected_resume=0;assert(!lgs_guest_backend.start(&s));
 f.expected_resume=1;f.dispatch_fail=1;assert(lgs_guest_backend.start(&s));assert(s.failed);
 unsigned entries=f.entries;assert(lgs_guest_backend.start(&s));assert(f.entries==entries);
 assert(!lgs_guest_backend.stop(&s));assert(!lgs_guest_backend.release(&s));
 f.prepare_fail=1;assert(lgs_guest_backend.prepare(&s,1));assert(s.memory.prepared);assert(!lgs_guest_backend.release(&s));f.prepare_fail=0;
 f.open_fail=1;assert(lgs_guest_backend.prepare(&s,1));assert(s.host.active);assert(!lgs_guest_backend.stop(&s));assert(!lgs_guest_backend.release(&s));f.open_fail=0;
 /* Fresh object: hook pair validation and callback ordering. */
 s=(struct lgs_session){0};f.dispatch_fail=0;p.before_entry=before;
 assert(!lgs_configure(&s,&h,&v,&p,&asset,1,&asset,1,"",0x1000,0x1000));
 p.after_entry=after;assert(lgs_configure(&s,&h,&v,&p,&asset,1,&asset,1,"",0x1000,0x1000));
 assert(!lgs_guest_backend.prepare(&s,1));f.expected_resume=0;f.before_fail=1;
 entries=f.entries;assert(lgs_guest_backend.start(&s));assert(f.entries==entries&&f.before==1&&f.after==0);
 assert(!lgs_guest_backend.release(&s));f.before_fail=0;
 assert(!lgs_guest_backend.prepare(&s,1));assert(!lgs_guest_backend.start(&s));
 assert(f.before==2&&f.after==1&&receipt.fields_valid&&receipt.reason==52&&receipt.state_result==LVS_EXIT);
 assert(!lgs_guest_backend.release(&s));
 assert(!lgs_guest_backend.prepare(&s,1));f.enter_fail=1;assert(lgs_guest_backend.start(&s));
 assert(f.after==2&&!receipt.fields_valid&&receipt.state_result==LVS_ENTRY_FAILED);
 assert(!lgs_guest_backend.release(&s));f.enter_fail=0;
 assert(!lgs_guest_backend.prepare(&s,1));f.custom_fields=true;f.reason=UINT64_C(1)<<31;
 assert(lgs_guest_backend.start(&s));assert(f.after==3&&receipt.fields_valid&&receipt.reason==(UINT32_C(1)<<31));
 assert(!lgs_guest_backend.release(&s));
 assert(!lgs_guest_backend.prepare(&s,1));f.reason=UINT64_C(1)<<32;
 assert(lgs_guest_backend.start(&s));assert(f.after==4&&!receipt.fields_valid);
 assert(!lgs_guest_backend.release(&s));f.custom_fields=false;
 assert(!lgs_guest_backend.prepare(&s,1));f.vectoring=UINT32_C(1)<<31;
 assert(lgs_guest_backend.start(&s));assert(f.after==5&&receipt.fields_valid&&receipt.vectoring==(UINT32_C(1)<<31));
 assert(!lgs_guest_backend.release(&s));f.vectoring=0;
 assert(!lgs_guest_backend.prepare(&s,1));f.read_fail=1;
 assert(lgs_guest_backend.start(&s));assert(f.after==6&&!receipt.fields_valid);
 assert(!lgs_guest_backend.release(&s));f.read_fail=0;
 assert(!lgs_guest_backend.prepare(&s,1));f.after_fail=1;
 assert(lgs_guest_backend.start(&s));assert(f.after==7&&s.failed);
 assert(!lgs_guest_backend.release(&s));f.after_fail=0;
 assert(!lgs_guest_backend.prepare(&s,1));s.native.entry_status=1;f.override_result=LVS_GUEST_CAPTURE_FAILED+1;
 assert(lgs_guest_backend.start(&s));assert(f.after==8&&!receipt.fields_valid&&receipt.state_result==LVS_GUEST_CAPTURE_FAILED);
 assert(!lgs_guest_backend.release(&s));f.override_result=0;
 assert(!lgs_guest_backend.prepare(&s,1));f.expected_resume=0;f.dirty=1;expect_fatal=true;
 unsigned after_before_dirty=f.after;
 unsigned frees_before=f.frees;
 if(!setjmp(fatal_jump)){(void)lgs_guest_backend.start(&s);assert(!"dirty host returned to caller");}
 assert(f.frees==frees_before&&s.host.active&&s.memory.prepared&&f.after==after_before_dirty);
 puts("linux_guest_session: retained VMCS stop/start, STEP, timer, cleanup barrier, recreate, failure no-resume PASS");
}
