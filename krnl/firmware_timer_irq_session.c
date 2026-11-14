#include "firmware_timer_irq_session.h"
static enum ftis_result fatal(struct ftis_session*s){
 if(s){s->failed=true;s->busy=false;fti_quarantine(&s->owner);}
 return FTIS_FATAL;
}
static bool valid(struct ftis_session*s,uint64_t d,uint64_t v){
 if(!s)return false;
 if(!s->ready||s->failed||s->busy||!d||!v||d!=s->domain||v!=s->vmcs){fatal(s);return false;}
 return true;
}
struct writer {struct ftis_session*s;void*context;lei_write write;};
static bool guarded_write(void*p,uint32_t field,uint64_t value){
 struct writer*w=p;
 if(w->s->failed||!w->s->busy)return false;
 bool ok=w->write(w->context,field,value);
 return ok&&!w->s->failed&&w->s->busy;
}
bool ftis_init(struct ftis_session*s,uint64_t d,uint64_t v,uint64_t r,uint64_t gap){
 if(!s||s->ready||s->failed||!d||!v)return false;
 if(!fti_init(&s->owner,d,r,gap))return false;
 lei_init(&s->event);s->domain=d;s->vmcs=v;s->ready=true;return true;
}
bool ftis_sample(struct ftis_session*s,uint64_t d,uint64_t v,uint64_t r){
 if(!valid(s,d,v))return false;
 if(!fti_sample(&s->owner,d,r)){fatal(s);return false;}return true;
}
enum ftis_result ftis_before(struct ftis_session*s,uint64_t d,uint64_t v,
 const struct lei_snapshot*g,void*c,lei_write w,uint64_t*out){
 if(!valid(s,d,v))return FTIS_FATAL;
 if(!g||!w||!out||s->active_attempt||s->event.phase!=LEI_IDLE)return fatal(s);
 if(!s->owner.pending||s->owner.in_service)return FTIS_IDLE;
 enum lei_gate gate=lei_check(&s->event,FTI_VECTOR,g);
 if(gate==LEI_DEFER_WINDOW)return FTIS_DEFER;
 if(gate!=LEI_READY||s->attempts==UINT64_MAX)return fatal(s);
 uint64_t token;uint8_t vector;
 if(!fti_reserve(&s->owner,&token,&vector))return fatal(s);
 s->active_attempt=++s->attempts;s->busy=true;
 struct writer writer={s,c,w};
 if(!lei_stage(&s->event,vector,token,g,&writer,guarded_write)||s->failed)return fatal(s);
 s->busy=false;*out=s->active_attempt;return FTIS_STAGED;
}
static bool account(void*c,uint64_t t,uint8_t v){
 struct ftis_session*s=c;
 return v==FTI_VECTOR&&fti_account(&s->owner,t,true);
}
enum ftis_result ftis_after(struct ftis_session*s,uint64_t d,uint64_t v,
 uint64_t attempt,bool vmfail,bool real_exit,uint32_t reason,uint32_t vectoring,
 void*c,lei_write w){
 if(!valid(s,d,v))return FTIS_FATAL;
 if(!w||!attempt||attempt!=s->active_attempt||s->event.phase!=LEI_STAGED||
    !s->event.armed)return fatal(s);
 s->busy=true;
 struct writer writer={s,c,w};
 lei_observe(&s->event,vmfail,real_exit,reason,vectoring);
 if(s->event.phase==LEI_STAGED&&s->event.cancellable){
  uint64_t token=s->event.token;
  if(!lei_cancel(&s->event,&writer,guarded_write)||s->failed||!fti_cancel(&s->owner,token,true))return fatal(s);
  s->active_attempt=0;
  /* No guest entry: original start has failed, no automatic reuse. */
  fatal(s);return FTIS_STOP_NO_ENTRY;
 }
 if(s->event.phase!=LEI_DELIVERED)return fatal(s);
 if(!lei_clear_delivered(&s->event,&writer,guarded_write)||s->failed||
    !lei_account(&s->event,s,account))return fatal(s);
 s->active_attempt=0;s->busy=false;return FTIS_ACCOUNTED;
}
bool ftis_eoi(struct ftis_session*s,uint64_t d,uint64_t v){
 if(!valid(s,d,v))return false;
 if(s->active_attempt||s->event.phase!=LEI_IDLE||!fti_eoi(&s->owner)){fatal(s);return false;}
 return true;
}
