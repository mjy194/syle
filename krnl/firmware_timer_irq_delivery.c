#include "firmware_timer_irq_delivery.h"
static enum ftis_result fail(struct ftis_session*s){
 if(s){s->failed=true;s->busy=false;fti_quarantine(&s->owner);
  if(s->event.phase==LEI_STAGED)s->event.phase=LEI_QUARANTINED;}
 return FTIS_FATAL;
}
struct writer {struct ftis_session*s;void*context;lei_write write;};
static bool write_guard(void*p,uint32_t f,uint64_t v){
 struct writer*w=p;if(w->s->failed||!w->s->busy)return false;
 bool ok=w->write(w->context,f,v);return ok&&!w->s->failed&&w->s->busy;
}
static bool accept(void*p,uint64_t t,uint8_t v){
 struct ftis_session*s=p;return v==FTI_VECTOR&&fti_account(&s->owner,t,true);
}
enum ftis_result ftid_after(struct ftis_session*s,const struct fine_outcome*o,void*c,lei_write w){
 if(!s)return FTIS_FATAL;
 if(!s->ready||s->failed||s->busy||!o||!w||!s->domain||!s->vmcs||
 !s->active_attempt||s->event.phase!=LEI_STAGED||!s->event.armed||
 s->event.cancellable||s->event.cleaned||s->event.vector!=FTI_VECTOR||
 !s->event.token||s->event.token!=s->owner.token||!s->owner.ready||
 s->owner.terminal||s->owner.domain!=s->domain||!s->owner.reserved||
 !s->owner.pending||s->owner.in_service)return fail(s);
 struct fir_identity identity={s->domain,s->vmcs,s->active_attempt};
 enum fir_result result=fine_classify(&identity,o);
 if(result==FIR_UNKNOWN)return fail(s);
 s->busy=true;struct writer writer={s,c,w};
 if(result==FIR_NO_ENTRY){
  uint64_t token=s->event.token;s->event.armed=false;s->event.cancellable=true;
  if(!lei_cancel(&s->event,&writer,write_guard)||s->failed||
     !fti_cancel(&s->owner,token,true))return fail(s);
  s->active_attempt=0;fail(s);return FTIS_STOP_NO_ENTRY;
 }
 /* Only this independently admitted bridge uses the richer classifier. */
 s->event.phase=LEI_DELIVERED;s->event.armed=false;
 if(!lei_clear_delivered(&s->event,&writer,write_guard)||s->failed||
    !lei_account(&s->event,s,accept))return fail(s);
 s->active_attempt=0;s->busy=false;return FTIS_ACCOUNTED;
}
