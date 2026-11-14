#include "../firmware_dirty_transaction.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct mock { struct fwd_plan p;unsigned calls,fail;char order[16];uint64_t leaf;enum fwd_cas_result conflict;bool uncertain_effect;};
static bool event(struct mock*m,char c){m->order[m->calls++]=c;return m->calls!=m->fail;}
static bool capture(void*x,struct fwd_plan*p){struct mock*m=x;if(!event(m,'C'))return false;*p=m->p;return true;}
static bool validate(void*x,const struct fwd_plan*p,bool post){struct mock*m=x;return event(m,post?'V':'P')&&m->leaf==(p->leaf_value|(post?0x40:0));}
static bool sample(void*x){return event(x,'S');}
static enum fwd_cas_result cas(void*x,uint64_t a,uint64_t old,uint64_t newv){struct mock*m=x;bool ok=event(m,'D');assert(a==m->p.leaf_entry_gpa&&newv==(old|0x40));if(m->uncertain_effect)m->leaf=newv;if(!ok)return FWD_CAS_UNCERTAIN;if(m->conflict)return m->conflict;if(m->leaf!=old)return FWD_CAS_CONFLICT;m->leaf=newv;return FWD_CAS_OK;}
static bool write_vm(void*x,uint32_t field,uint64_t value){struct mock*m=x;assert((field==0x6820&&value==0x46)||(field==0x681e&&value==m->p.next_rip));return event(m,field==0x6820?'F':'R');}
static struct mock fresh(void){return (struct mock){.p={0x7c06ff0,0xffc000a3,0x8000,0x8003,0x10046},.leaf=0xffc000a3};}
int main(void){struct fwd_ops o={capture,validate,cas,sample,write_vm};struct mock m=fresh();struct fwd_state s={0};assert(fwd_apply(&s,true,true,&m,&o)&&s.complete&&s.dirty_changed&&!s.failed&&m.leaf==0xffc000e3&&!memcmp(m.order,"CPSDVSFR",8));unsigned calls=m.calls;assert(!fwd_apply(&s,true,true,&m,&o)&&m.calls==calls);
 for(unsigned i=1;i<=8;i++){m=fresh();m.fail=i;s=(struct fwd_state){0};assert(!fwd_apply(&s,true,true,&m,&o)&&s.failed&&!s.complete&&m.calls==i);assert(m.leaf==(i>4?0xffc000e3:0xffc000a3));calls=m.calls;assert(!fwd_apply(&s,true,true,&m,&o)&&m.calls==calls);}
 m=fresh();m.p.leaf_value=m.leaf=0xffc000e3;s=(struct fwd_state){0};assert(fwd_apply(&s,true,true,&m,&o)&&!s.dirty_changed&&!memcmp(m.order,"CPSVSFR",7));
 m=fresh();s=(struct fwd_state){0};assert(!fwd_apply(&s,false,true,&m,&o)&&!s.consumed&&!m.calls);assert(!fwd_apply(&s,true,false,&m,&o)&&s.failed&&!m.calls);
 for(unsigned bit=0;bit<64;bit++){if(bit==6)continue;m=fresh();m.leaf^=UINT64_C(1)<<bit;s=(struct fwd_state){0};assert(!fwd_apply(&s,true,true,&m,&o)&&s.failed&&m.calls==2);}
 m=fresh();m.conflict=FWD_CAS_CONFLICT;s=(struct fwd_state){0};assert(!fwd_apply(&s,true,true,&m,&o)&&s.failed&&m.calls==4&&m.leaf==0xffc000a3);
 m=fresh();m.conflict=FWD_CAS_UNCERTAIN;m.uncertain_effect=true;s=(struct fwd_state){0};assert(!fwd_apply(&s,true,true,&m,&o)&&s.failed&&m.leaf==0xffc000e3);
 m=fresh();m.p.leaf_value&=~0x20ULL;s=(struct fwd_state){0};assert(!fwd_apply(&s,true,true,&m,&o)&&m.calls==1&&s.failed);
 puts("dirty transaction: host-only order, exact CAS, D1 bypass, irreversible failure, one-shot PASS");return 0;}
