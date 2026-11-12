#include "ad_updater_gate.h"
static bool same(struct ad_entry_stamp a,struct ad_entry_stamp b)
{return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.context_id==b.context_id&&a.epoch==b.epoch&&a.lifetime==b.lifetime;}
static bool locked(struct ad_updater_gate *g,bool cleanup)
{
 if(!g||!g->initialized||(!cleanup&&__atomic_load_n(&g->fault,__ATOMIC_ACQUIRE)))return false;
 for(unsigned i=0;i<AD_UPDATER_LOCK_TRIES;i++){
  uint32_t zero=0;
  if(__atomic_compare_exchange_n(&g->lock,&zero,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){
   if(!cleanup&&__atomic_load_n(&g->fault,__ATOMIC_ACQUIRE)){
    __atomic_store_n(&g->lock,0,__ATOMIC_RELEASE);return false;
   }
   return true;
  }
  __asm__ volatile("pause":::"memory");
 }
 __atomic_store_n(&g->fault,1,__ATOMIC_RELEASE);return false;
}
static void unlock(struct ad_updater_gate*g){__atomic_store_n(&g->lock,0,__ATOMIC_RELEASE);}
bool ad_updater_gate_init(struct ad_updater_gate*g,struct ad_entry_stamp s)
{
 if(!g||!s.domain_id||!s.incarnation||!s.context_id||!s.epoch||!s.lifetime)return false;
 *g=(struct ad_updater_gate){.stamp=s,.initialized=true};return true;
}
bool ad_updater_gate_init_successor(struct ad_updater_gate *n,
 struct ad_updater_gate *g,struct ad_entry_stamp old,struct ad_entry_stamp next,
 uint64_t reserve)
{
 if(!n||!g||((uintptr_t)n>=(uintptr_t)g?
    (uintptr_t)n-(uintptr_t)g<sizeof(*g):(uintptr_t)g-(uintptr_t)n<sizeof(*n))||
    n->initialized||!reserve||
    !next.domain_id||!next.incarnation||!next.context_id||!next.epoch||!next.lifetime||
    next.domain_id!=old.domain_id||next.incarnation!=old.incarnation||
    next.context_id==old.context_id||next.lifetime==old.lifetime||!locked(g,false))return false;
 bool ok=same(g->stamp,old)&&g->closed&&!g->superseded&&!g->references&&!g->tokens&&
  reserve<=UINT64_MAX-g->next_nonce;
 if(ok){*n=(struct ad_updater_gate){.predecessor=g,.stamp=next,.initialized=true,.next_nonce=g->next_nonce};g->superseded=true;}
 unlock(g);return ok;
}
bool ad_updater_gate_acquire(struct ad_updater_gate*g,struct ad_entry_stamp s,struct ad_updater_token*t,uint64_t *nonce_out)
{
 if(!t||!nonce_out||!locked(g,false))return false;
 bool ok=false;
 if(!same(s,g->stamp)||g->closed||t->live||g->references==UINT64_MAX||g->next_nonce==UINT64_MAX)goto done;
 for(struct ad_updater_token*p=g->tokens;p;p=p->next)if(p==t)goto done;
 *t=(struct ad_updater_token){.gate=g,.next=g->tokens,.stamp=s,.nonce=g->next_nonce+1,.live=true};
 g->tokens=t;g->references++;g->next_nonce=t->nonce;*nonce_out=t->nonce;ok=true;
 done:unlock(g);return ok;
}
bool ad_updater_gate_release(struct ad_updater_gate*g,struct ad_updater_token*t,struct ad_entry_stamp s,uint64_t nonce)
{
 if(!t||!locked(g,true))return false;
 bool ok=false;
 if(!same(s,g->stamp)||!t->live||!nonce||t->nonce!=nonce||t->gate!=g||!same(t->stamp,s)||!g->references)goto done;
 struct ad_updater_token **link=&g->tokens;
 while(*link&&*link!=t)link=&(*link)->next;
 if(!*link)goto done;
 *link=t->next;g->references--;t->live=false;t->next=0;t->gate=0;ok=true;
 done:unlock(g);return ok;
}
bool ad_updater_gate_close(struct ad_updater_gate*g,struct ad_entry_stamp s)
{
 if(!locked(g,false))return false;
 bool ok=same(s,g->stamp)&&!g->closed;
 if(ok)g->closed=true;
 unlock(g);return ok;
}
bool ad_updater_gate_drained(struct ad_updater_gate*g,struct ad_entry_stamp s)
{
 if(!locked(g,false))return false;
 bool ok=same(s,g->stamp)&&g->closed&&!g->references&&!g->tokens;
 unlock(g);return ok;
}
bool ad_updater_gate_reopen(struct ad_updater_gate*g,struct ad_entry_stamp s,bool invalidated,struct ad_entry_stamp*out)
{
 if(!out||!locked(g,false))return false;
 bool ok=!g->superseded&&invalidated&&same(s,g->stamp)&&g->closed&&!g->references&&!g->tokens&&s.epoch!=UINT64_MAX;
 if(ok){g->stamp.epoch++;g->closed=false;*out=g->stamp;}
 unlock(g);return ok;
}
void ad_updater_gate_poison(struct ad_updater_gate*g)
{if(g)__atomic_store_n(&g->fault,1,__ATOMIC_RELEASE);}

bool ad_updater_gate_guard(struct ad_updater_gate*g,struct ad_updater_token*t,struct ad_entry_stamp s,uint64_t nonce)
{
 if(!t||!locked(g,false))return false;
 bool ok=false;
 if(same(s,g->stamp)&&t->live&&nonce&&t->nonce==nonce&&t->gate==g&&same(t->stamp,s))
  for(struct ad_updater_token*p=g->tokens;p;p=p->next)if(p==t){ok=true;break;}
 unlock(g);return ok;
}

bool ad_updater_gate_idle(struct ad_updater_gate*g,struct ad_entry_stamp s)
{
 if(!locked(g,false))return false;
 bool ok=same(s,g->stamp)&&!g->closed&&!g->references&&!g->tokens;
 unlock(g);return ok;
}

bool ad_updater_gate_open(struct ad_updater_gate*g,struct ad_entry_stamp s)
{
 if(!locked(g,false))return false;
 bool ok=same(s,g->stamp)&&!g->closed;
 unlock(g);return ok;
}

bool ad_updater_gate_successor_budget(struct ad_updater_gate *g,struct ad_entry_stamp s,uint64_t reserve,uint64_t *nonce){
 if(!nonce||!reserve||!locked(g,false))return false;
 bool ok=same(s,g->stamp)&&g->closed&&!g->superseded&&!g->references&&!g->tokens&&reserve<=UINT64_MAX-g->next_nonce;
 if(ok)*nonce=g->next_nonce;
 unlock(g);return ok;
}
