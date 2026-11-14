#include "firmware_irq_window.h"
static enum fiw_result fail(struct fiw_state*s){if(s)s->failed=true;return FIW_FATAL;}
static bool identity(struct fiw_state*s,uint64_t d,uint64_t v){return s&&s->ready&&!s->failed&&!s->busy&&d==s->domain&&v==s->vmcs&&d&&v;}
bool fiw_init(struct fiw_state*s,uint64_t d,uint64_t v,uint32_t base,uint64_t caps){
 if(!s||s->ready||s->failed||!d||!v||(base&4)||(caps&4)||!((caps>>32)&4))return false;
 *s=(struct fiw_state){.domain=d,.vmcs=v,.base=base,.ready=true};return true;
}
enum fiw_result fiw_before(struct fiw_state*s,uint64_t d,uint64_t v,uint32_t actual,
 bool pending,bool priority,const struct lei_snapshot*g,void*c,lei_write write,uint64_t*out){
 if(!identity(s,d,v)||s->armed||!g||!write||!out||actual!=s->base)return fail(s);
 struct lei_irq idle={0};enum lei_gate gate=lei_check(&idle,32,g);
 if(gate==LEI_REJECT)return fail(s);
 if(!pending||!priority)return FIW_WAIT;
 if(gate==LEI_READY)return FIW_READY;
 if(s->generation==UINT64_MAX)return fail(s);
 s->attempt=++s->generation;s->busy=true;
 bool ok=write(c,0x4002,s->base|4);s->busy=false;
 if(!ok||s->failed)return fail(s);
 s->armed=true;*out=s->attempt;return FIW_ARMED;
}
enum fiw_result fiw_after(struct fiw_state*s,uint64_t d,uint64_t v,uint64_t attempt,
 uint32_t actual,uint32_t reason,bool safe_exit,void*c,lei_write write){
 if(!identity(s,d,v)||!s->armed||!attempt||attempt!=s->attempt||actual!=(s->base|4)||
 !safe_exit||!write||(reason&~UINT32_C(0xffff)))return fail(s);
 s->busy=true;bool ok=write(c,0x4002,s->base);s->busy=false;
 if(!ok||s->failed)return fail(s);
 s->armed=false;s->attempt=0;return FIW_RECHECK;
}
