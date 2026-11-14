#include "firmware_dirty_transaction.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool shape(const struct fwd_plan*p){
 return !(p->leaf_entry_gpa&7)&&p->leaf_entry_gpa<UINT64_C(0x8000000)&&
 (p->leaf_value&0xa7)==0xa3&&!(p->leaf_value&UINT64_C(0xffffffff001fe000))&&
 canonical(p->rip)&&canonical(p->next_rip)&&p->next_rip>p->rip&&p->next_rip-p->rip<=15&&
 (p->rflags&2)&&!(p->rflags&~UINT64_C(0x250ed7));
}
bool fwd_apply(struct fwd_state*s,bool exclusive,bool c2,void*ctx,const struct fwd_ops*o){
 if(!exclusive||!s||s->failed||s->consumed)return false;
 s->consumed=true;struct fwd_plan p={0};
 if(!c2||!o||!o->capture||!o->validate||!o->cas||!o->sample||!o->write||
 !o->capture(ctx,&p)||!shape(&p)||!o->validate(ctx,&p,false)||!o->sample(ctx))goto fail;
 if(!(p.leaf_value&0x40)){
  if(o->cas(ctx,p.leaf_entry_gpa,p.leaf_value,p.leaf_value|0x40)!=FWD_CAS_OK)goto fail;
  s->dirty_changed=true;
 }
 if(!o->validate(ctx,&p,true)||!o->sample(ctx)||
 ((p.rflags&0x10000)&&!o->write(ctx,0x6820,p.rflags&~UINT64_C(0x10000)))||
 !o->write(ctx,0x681e,p.next_rip))goto fail;
 s->complete=true;return true;
fail:s->failed=true;return false;
}
