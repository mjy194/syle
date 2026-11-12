#include "pic_master_04_glue.h"
static bool valid(const struct pmg*s,const struct pic_master_04*p){
 if(!s||!p||s->domain!=p->domain)return false;
 struct pic_master_04 copy=*p;return pm04_edges0(&copy,p->domain,p->seen0);
}
static bool fail(struct pmg*s){s->failed=true;s->busy=false;return false;}
bool pmg_init(struct pmg*s,const struct pic_master_04*p){
 if(!s||!p||p->reserved.serial)return false;
 struct pic_master_04 copy=*p;if(!pm04_edges0(&copy,p->domain,p->seen0))return false;
 *s=(struct pmg){.domain=p->domain};return true;
}
enum pmg_before_result pmg_before(struct pmg*s,struct pic_master_04*p,const struct lei_snapshot*g,void*c,lei_write w){
 if(!s||s->failed||s->busy)return PMG_ERROR;
 s->busy=true;
 if(!g||!w||!valid(s,p)||s->event.phase!=LEI_IDLE||p->reserved.serial||g->vectoring||(!(g->pf_pending||g->high_pending)&&g->entry_info)){fail(s);return PMG_ERROR;}
 if(g->pf_pending||g->high_pending){s->busy=false;return PMG_READY;}
 uint8_t source,v;if(!pm04_peek(p,&source,&v)){s->busy=false;return PMG_READY;}
 enum lei_gate gate=lei_check(&s->event,v,g);
 if(gate==LEI_DEFER_WINDOW){s->busy=false;return PMG_DEFER_WINDOW;}
 if(gate!=LEI_READY||!pm04_reserve(p,&s->ticket)||!lei_stage(&s->event,v,s->ticket.serial,g,c,w)){fail(s);return PMG_ERROR;}
 s->busy=false;return PMG_READY;
}
struct account {struct pic_master_04*p;const struct pm04_ticket*t;};
static bool accept(void*x,uint64_t token,uint8_t vector){
 struct account*a=x;return token==a->t->serial&&vector==a->t->vector&&pm04_accept(a->p,a->t);
}
bool pmg_after(struct pmg*s,struct pic_master_04*p,enum pmg_outcome o,uint32_t reason,uint32_t vectoring,void*c,lei_write w){
 if(!s||s->failed||s->busy)return false;
 s->busy=true;
 if(!w||!valid(s,p)||(unsigned)o>PMG_UNKNOWN)return fail(s);
 if(s->event.phase==LEI_IDLE){if(p->reserved.serial)return fail(s);s->busy=false;return true;}
 if(o==PMG_NO_ENTRY&&(reason||vectoring))return fail(s);
 lei_observe(&s->event,o==PMG_NO_ENTRY,o==PMG_EXIT,reason,vectoring);
 if(s->event.phase==LEI_STAGED&&s->event.cancellable){
  struct lei_irq clear=s->event;
  if(!lei_cancel(&clear,c,w)||!pm04_cancel(p,&s->ticket))return fail(s);
  s->event=clear;s->ticket=(struct pm04_ticket){0};s->busy=false;return true;
 }
 if(s->event.phase==LEI_QUARANTINED){(void)pm04_quarantine(p,&s->ticket);return fail(s);}
 struct account a={p,&s->ticket};
 if(s->event.phase!=LEI_DELIVERED||!lei_clear_delivered(&s->event,c,w)||!lei_account(&s->event,&a,accept))return fail(s);
 s->ticket=(struct pm04_ticket){0};s->busy=false;return true;
}
