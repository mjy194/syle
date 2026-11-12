#include "linux_external_irq.h"
void lei_init(struct lei_irq *s) { if(s) *s=(struct lei_irq){0}; }
enum lei_gate lei_check(const struct lei_irq *s,unsigned v,const struct lei_snapshot *g) {
 if(!s||!g||s->phase!=LEI_IDLE||v<32||v>255||g->activity||g->entry_info||g->vectoring||g->pf_pending||g->high_pending||(g->blocking&~3u)) return LEI_REJECT;
 return (!(g->rflags&(1ull<<9))||(g->blocking&3))?LEI_DEFER_WINDOW:LEI_READY;
}
bool lei_stage(struct lei_irq *s,unsigned v,uint64_t token,const struct lei_snapshot *g,void *c,lei_write w) {
 if(!token||!w||lei_check(s,v,g)!=LEI_READY) return false;
 *s=(struct lei_irq){LEI_STAGED,token,(uint8_t)v,false,true,false};
 /* Valid last. Any failed write forbids entry, including uncertain writes. */
 if(!w(c,0x4018,0)||!w(c,0x401a,0)||!w(c,0x4016,0x80000000u|v)) return false;
 s->armed=true; s->cancellable=false; return true;
}
void lei_observe(struct lei_irq *s,bool vmfail,bool real_exit,uint32_t reason,uint32_t vectoring) {
 if(!s||s->phase!=LEI_STAGED||!s->armed) return;
 if((vmfail&&!real_exit&&!reason&&!vectoring)||(!vmfail&&real_exit&&(reason==0x80000021u||reason==0x80000022u)&&!vectoring)) {
  s->armed=false; s->cancellable=true; return;
 }
 if(vmfail||!real_exit||vectoring||(reason&~0xffffu)) { s->phase=LEI_QUARANTINED; return; }
 /* These intercepts require execution of a guest instruction after entry.
  * Never infer delivery from timer, EPT, exception, or entry-info valid. */
 switch(reason) {
 case 10: case 28: case 30: case 31: case 32: case 12:
  s->phase=LEI_DELIVERED; s->armed=false; return;
 default: s->phase=LEI_QUARANTINED; return;
 }
}
bool lei_cancel(struct lei_irq *s,void *c,lei_write w) {
 if(!s||!w||s->phase!=LEI_STAGED||s->armed||!s->cancellable) return false;
 if(!w(c,0x4016,0)||!w(c,0x4018,0)||!w(c,0x401a,0)) return false;
 lei_init(s); return true;
}
bool lei_clear_delivered(struct lei_irq *s,void *c,lei_write w) {
 if(!s||!w||s->phase!=LEI_DELIVERED) return false;
 if(s->cleaned) return true;
 if(!w(c,0x4016,0)||!w(c,0x4018,0)||!w(c,0x401a,0)) return false;
 s->cleaned=true; return true;
}
bool lei_account(struct lei_irq *s,void *c,lei_accept a) {
 if(!s||!a||s->phase!=LEI_DELIVERED||!s->cleaned) return false;
 if(!a(c,s->token,s->vector)) return false;
 lei_init(s); return true;
}
