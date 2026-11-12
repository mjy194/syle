#include "pic0_glue.h"
static bool valid(struct pgg*s){if(!s||!s->pic||s->pic->domain!=s->domain)return false;struct pic0 copy=*s->pic;return pic0_edges(&copy,s->domain,copy.seen);}
static bool fail(struct pgg*s){s->failed=true;s->busy=false;return false;}
bool pgg_init(struct pgg*s,struct pic0*p){if(!s||!p)return false;struct pic0 copy=*p;if(p->reserved||!pic0_edges(&copy,p->domain,p->seen))return false;*s=(struct pgg){.pic=p,.domain=p->domain};return true;}
enum pgg_before_result pgg_before(struct pgg*s,const struct lei_snapshot*g,void*c,lei_write w){
 if(!s||s->failed||s->busy)return PGG_ERROR;
 s->busy=true;
 if(!g||!w||!valid(s)||s->event.phase!=LEI_IDLE||s->pic->reserved){fail(s);return PGG_ERROR;}
 if(g->pf_pending||g->high_pending){s->busy=false;return PGG_READY;}
 uint8_t v;if(!pic0_peek(s->pic,&v)){s->busy=false;return PGG_READY;}
 enum lei_gate gate=lei_check(&s->event,v,g);
 if(gate==LEI_DEFER_WINDOW){s->busy=false;return PGG_DEFER_WINDOW;}
 uint64_t token;if(gate!=LEI_READY||!pic0_reserve(s->pic,&token,&v)||!lei_stage(&s->event,v,token,g,c,w)){fail(s);return PGG_ERROR;}
 s->busy=false;return PGG_READY;
}
static bool accept(void*p,uint64_t t,uint8_t v){struct pgg*s=p;return pic0_accept(s->pic,s->domain,t,v);}
bool pgg_after(struct pgg*s,enum pgg_outcome o,uint32_t reason,uint32_t vectoring,void*c,lei_write w){
 if(!s||s->failed||s->busy)return false;
 s->busy=true;
 if(!w||!valid(s)||(unsigned)o>PGG_UNKNOWN)return fail(s);
 if(s->event.phase==LEI_IDLE){if(s->pic->reserved)return fail(s);s->busy=false;return true;}
 if(o==PGG_NO_ENTRY&&(reason||vectoring))return fail(s);
 lei_observe(&s->event,o==PGG_NO_ENTRY,o==PGG_EXIT,reason,vectoring);
 if(s->event.phase==LEI_STAGED&&s->event.cancellable){
  uint64_t t=s->event.token;uint8_t v=s->event.vector;struct lei_irq clear=s->event;
  if(!lei_cancel(&clear,c,w)||!pic0_cancel(s->pic,s->domain,t,v))return fail(s);
  s->event=clear;s->busy=false;return true;
 }
 if(s->event.phase!=LEI_DELIVERED||!lei_clear_delivered(&s->event,c,w)||!lei_account(&s->event,s,accept))return fail(s);
 s->busy=false;return true;
}
