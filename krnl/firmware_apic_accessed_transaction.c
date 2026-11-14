#include "firmware_apic_accessed_transaction.h"
enum fat_result faat_apply(struct faat_state*s,struct fct_state*a,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,uint32_t ecx,uint32_t edx,
 bool exclusive,bool c2,void*ctx,bool(*sample)(void*),
 enum fwd_cas_result(*cas)(void*,uint64_t*,uint64_t,uint64_t),bool(*write)(void*,uint32_t,uint64_t)){
 if(!exclusive||!s||!a)return FAT_REJECTED;
 if(s->failed){a->failed=true;a->clock.timer.terminal=true;return FAT_FATAL;}
 if(s->consumed||a->failed||a->clock.timer.terminal||!c2||!sample||!cas||!write)return FAT_REJECTED;
 if(!faa_capture(m,e,g,a,ecx,edx,&s->plan))return FAT_REJECTED;
 s->consumed=true;
 if(!faa_validate(&s->plan,false)||!sample(ctx))goto fail;
 if(faa_compare_set(&s->plan,ctx,cas)!=FWD_CAS_OK)goto fail;
 s->accessed_known=true;
 if(!faa_validate(&s->plan,true)||!sample(ctx)||!faa_validate(&s->plan,true)||
 ((e->rflags&0x10000)&&!write(ctx,0x6820,e->rflags&~UINT64_C(0x10000)))||
 !write(ctx,0x681e,s->plan.transaction.next_rip))goto fail;
 g[0]=s->plan.new_rax;s->complete=true;return FAT_COMMITTED;
fail:s->failed=true;a->failed=true;a->clock.timer.terminal=true;return FAT_FATAL;
}
