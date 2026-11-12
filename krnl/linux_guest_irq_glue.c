#include "linux_guest_irq_glue.h"
static bool fail(struct lig_state*s,struct gia*a){s->failed=true;if(s->event.phase==LEI_QUARANTINED)gia_quarantine(a);s->busy=false;return false;}
bool lgig_before(struct lig_state*s,struct gia*a,const struct lei_snapshot*g,void*c,lei_write w){
 if(!s||!a||!g||!w||s->busy||s->failed)return false;
 s->busy=true;
 if(s->event.phase!=LEI_IDLE)return fail(s,a);
 /* Pending PF/highPF owns VM entry; it must not be overwritten. */
 if(g->pf_pending||g->high_pending){s->busy=false;return true;}
 uint8_t vector;uint64_t token;enum gia_source source;
 if(!gia_peek(a,&vector,&source)){s->busy=false;return true;}
 enum lei_gate gate=lei_check(&s->event,vector,g);
 if(gate==LEI_DEFER_WINDOW){s->busy=false;return true;}
 if(gate!=LEI_READY||!gia_reserve(a,&vector,&token,&source))return fail(s,a);
 if(!lei_stage(&s->event,vector,token,g,c,w))return fail(s,a);
 s->busy=false;return true;
}
bool lgig_after(struct lig_state*s,struct gia*a,enum lig_outcome o,uint32_t reason,uint32_t vector,void*c,lei_write w){
 if(!s||!a||!w||s->busy||s->failed)return false;
 s->busy=true;
 if(s->event.phase==LEI_IDLE){s->busy=false;return true;}
 if(o==LIG_NO_ENTRY&&(reason||vector))return fail(s,a);
 lei_observe(&s->event,o==LIG_NO_ENTRY,o==LIG_EXIT,reason,vector);
 if(s->event.phase==LEI_STAGED&&s->event.cancellable){
  uint64_t token=s->event.token;uint8_t v=s->event.vector;
  struct lei_irq cleared=s->event;
  if(!lei_cancel(&cleared,c,w)||!gia_cancel(a,token,v))return fail(s,a);
  s->event=cleared;
  /* Guest did not enter. Caller must fail the original start, not resume. */
  s->busy=false;return true;
 }
 if(s->event.phase!=LEI_DELIVERED)return fail(s,a);
 if(!lei_clear_delivered(&s->event,c,w)||!lei_account(&s->event,a,gia_accept))return fail(s,a);
 s->busy=false;return true;
}
