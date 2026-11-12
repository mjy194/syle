#include "nested_source_replace_chain.h"
static uint64_t take64(const uint64_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void give64(uint64_t *p,uint64_t v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
static bool zero(const void *p,size_t n){const unsigned char *s=p;for(size_t i=0;i<n;i++)if(s[i])return false;return true;}
static bool disjoint(const void *a,size_t na,const void *b,size_t nb){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&na&&nb&&na<=UINTPTR_MAX-x&&nb<=UINTPTR_MAX-y&&(x+na<=y||y+nb<=x);
}
static bool identity(struct nrc_identity a,struct nrc_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;
}
static bool stamp(struct ad_entry_stamp a,struct ad_entry_stamp b){
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.context_id==b.context_id&&a.epoch==b.epoch&&a.lifetime==b.lifetime;
}
static bool bank_live(const struct nsrc_bank *b){
 return b->catalog&&b->gate&&b->catalog->published&&identity(b->catalog->identity,b->identity)&&stamp(b->gate->stamp,b->stamp)&&
  b->catalog->spans==b->spans&&b->catalog->roots==b->roots&&
  b->catalog->span_capacity==b->span_capacity&&b->catalog->root_capacity==b->root_capacity;
}
static bool banks(const struct nsrc_bank b[3],struct nested_source_replace_chain *s,
 struct nested_source_replace_round *r0,struct nested_source_replace_round *r1){
 const void *ptr[12];size_t size[12];
 for(unsigned i=0;i<3;i++){
  if(!b[i].identity.domain||!b[i].identity.incarnation||!b[i].identity.catalog||!b[i].identity.lifetime||
   !b[i].stamp.context_id||!b[i].stamp.epoch||!b[i].stamp.lifetime||
   b[i].stamp.domain_id!=b[i].identity.domain||b[i].stamp.incarnation!=b[i].identity.incarnation||
   b[i].stamp.context_id!=b[i].identity.catalog||b[i].stamp.lifetime!=b[i].identity.lifetime||
   !b[i].span_capacity||b[i].span_capacity>SIZE_MAX/sizeof *b[i].spans||
   !b[i].root_capacity||b[i].root_capacity>SIZE_MAX/sizeof *b[i].roots)return false;
  ptr[4*i]=b[i].catalog;size[4*i]=sizeof *b[i].catalog;
  ptr[4*i+1]=b[i].gate;size[4*i+1]=sizeof *b[i].gate;
  ptr[4*i+2]=b[i].spans;size[4*i+2]=b[i].span_capacity*sizeof *b[i].spans;
  ptr[4*i+3]=b[i].roots;size[4*i+3]=b[i].root_capacity*sizeof *b[i].roots;
  for(unsigned j=0;j<i;j++)if(identity(b[i].identity,b[j].identity)||stamp(b[i].stamp,b[j].stamp)||
   b[i].identity.domain!=b[j].identity.domain||b[i].identity.incarnation!=b[j].identity.incarnation||
   b[i].identity.catalog==b[j].identity.catalog||b[i].identity.lifetime==b[j].identity.lifetime||
   b[i].stamp.context_id==b[j].stamp.context_id||b[i].stamp.lifetime==b[j].stamp.lifetime||b[i].stamp.epoch<=b[j].stamp.epoch)return false;
 }
 for(unsigned i=0;i<12;i++){
  if(!disjoint(ptr[i],size[i],s,sizeof *s)||!disjoint(ptr[i],size[i],r0,sizeof *r0)||!disjoint(ptr[i],size[i],r1,sizeof *r1))return false;
  for(unsigned j=0;j<i;j++)if(!disjoint(ptr[i],size[i],ptr[j],size[j]))return false;
 }
 return bank_live(&b[0])&&zero(b[1].catalog,sizeof *b[1].catalog)&&zero(b[2].catalog,sizeof *b[2].catalog)&&
  zero(b[1].gate,sizeof *b[1].gate)&&zero(b[2].gate,sizeof *b[2].gate);
}
static bool change(void *p,struct tsr_ticket t,struct nested_root_catalog *old,struct ad_updater_gate *gate,
 struct nested_root_catalog **out,struct ad_updater_gate **gout,void *contexts[2]){
 struct nsrc_change_context *c=p;struct nested_source_replace_chain *s=c->chain;unsigned n=c->index;
 if(n>1||s->self!=s||take64(&s->fault)||take64(&s->active_generation)!=n+1||
    old!=s->banks[n].catalog||gate!=s->banks[n].gate||!identity(t.source,s->banks[n].identity)||!stamp(t.stamp,s->banks[n].stamp))return false;
 if(!s->change[n](s->change_context[n],t,old,gate,out,gout,contexts))return false;
 return *out==s->banks[n+1].catalog&&*gout==s->banks[n+1].gate&&bank_live(&s->banks[n+1])&&
  (*out)->predecessor==old&&(*gout)->predecessor==gate;
}
bool nsrc_init(struct nested_source_replace_chain *s,struct nested_source_replace_round *r0,
 struct nested_source_replace_round *r1,const struct nsrc_bank b[3]){
 if(!s||!r0||!r1||!b||r0==r1||!disjoint(s,sizeof *s,r0,sizeof *r0)||!disjoint(s,sizeof *s,r1,sizeof *r1)||
    !disjoint(r0,sizeof *r0,r1,sizeof *r1)||!zero(s,sizeof *s)||!r0->bootstrapped||r1->bootstrapped||
    !r0->metadata_lock||!r0->metadata_unlock||!r0->change||!r1->change||r0->dispatch!=r1->dispatch||
    r0->active_generation||r1->active_generation||!zero(&r1->replacement,sizeof r1->replacement)||
    !banks(b,s,r0,r1))return false;
 for(unsigned c=0;c<2;c++)if(!r0->owners[c]||r0->owners[c]!=r1->owners[c]||r0->arrived[c]||r1->arrived[c]||
   r0->refs[c].phase!=NOSR_READY||r1->refs[c].phase!=NOSR_FRESH||
   r0->replacement.old_catalog!=b[0].catalog||r0->replacement.old_gate!=b[0].gate)return false;
 void (*lock_fn)(void *)=r0->metadata_lock,(*unlock_fn)(void *)=r0->metadata_unlock;
 void *context=r0->metadata_context;
 lock_fn(context);
 bool storage=tsr_successor_storage_locked(&r0->replacement,s,sizeof *s)&&
  tsr_successor_storage_locked(&r0->replacement,r1,sizeof *r1);
 for(unsigned i=1;i<3&&storage;i++)storage=
  tsr_successor_storage_locked(&r0->replacement,b[i].catalog,sizeof *b[i].catalog)&&
  tsr_successor_storage_locked(&r0->replacement,b[i].gate,sizeof *b[i].gate)&&
  tsr_successor_storage_locked(&r0->replacement,b[i].spans,b[i].span_capacity*sizeof *b[i].spans)&&
  tsr_successor_storage_locked(&r0->replacement,b[i].roots,b[i].root_capacity*sizeof *b[i].roots);
 bool ok=storage&&tsr_bind_successor_locked(&s->handoff,&r0->replacement,&r1->replacement,&r0->refs[0],&r0->refs[1],&s->active_generation,1);
 if(ok){
  s->lock=lock_fn;s->unlock=unlock_fn;s->context=context;s->current_cpu=r0->refs[0].current_cpu;
  s->self=s;s->rounds[0]=r0;s->rounds[1]=r1;
  for(unsigned i=0;i<3;i++)s->banks[i]=b[i];
  for(unsigned c=0;c<2;c++){
   s->owner_config[c]=r0->refs[c].config;s->owner_config[c].replacement=&r1->replacement;
   s->change[c]=s->rounds[c]->change;s->change_context[c]=s->rounds[c]->change_context;
   s->changes[c]=(struct nsrc_change_context){s,c};
   s->rounds[c]->change=change;s->rounds[c]->change_context=&s->changes[c];
   s->rounds[c]->active_generation=&s->active_generation;s->rounds[c]->generation=c+1;s->rounds[c]->chain_fault=&s->fault;
  }
  r1->metadata_lock=s->lock;r1->metadata_unlock=s->unlock;r1->metadata_context=s->context;
 }
 unlock_fn(context);return ok;
}
static bool healthy(struct nested_source_replace_chain *s){
 return !take64(&s->fault)&&(!s->rounds[0]->healthy||s->rounds[0]->healthy(s->rounds[0]->context));
}
static bool wait64(struct nested_source_replace_chain *s,uint64_t *p,uint64_t expected){
 for(unsigned i=0;i<NSRR_POLLS;i++){
  if(!healthy(s))return false;
  uint64_t got=take64(p);if(got==expected)return true;if(got>expected)return false;
  __asm__ volatile("pause":::"memory");
 }return false;
}
static bool transition_fail(struct nested_source_replace_chain *s){
 /* While preparing next generation, old is still the CURRENT authority.
  * Its fail latch serializes with publication, so failure cannot be dropped. */
 s->lock(s->context);
 uint64_t g=take64(&s->active_generation);
 /* Selection and latch are indivisible with successor publication. This is
  * an actual in-flight handoff failure, unlike a stale old-round callback. */
 if(g>=1&&g<=2)give64(&s->fault,1);
 s->unlock(s->context);
 if(g>=1&&g<=2)return nsrr_retain_stop(s->rounds[g-1]);
 return false;
}
static bool successor(struct nested_source_replace_chain *s,unsigned c){
 s->lock(s->context);
 bool ok=take64(&s->active_generation)==1&&!take64(&s->fault)&&!s->transition[c]&&
  s->handoff.departed[c]==1;
 if(ok)give64(&s->transition[c],2);
 s->unlock(s->context);
 if(!ok)return false;
 if(!wait64(s,&s->handoff.departed[0],1)||!wait64(s,&s->handoff.departed[1],1)||
    !wait64(s,&s->transition[1-c],2))return transition_fail(s);
 struct nested_source_replace_round *r=s->rounds[1];struct tsr_receipt receipt;
 /* Each CPU initializes/audits only its own fresh adapter. Both CPUs have
  * now stopped normal owner execution. No BSP writes of peer-local state. */
 if(!nosr_init(&r->refs[c],&s->owner_config[c])||
    !nosr_checkpoint(&r->refs[c],s->handoff.prior,s->handoff.ticket,&receipt))return transition_fail(s);
 s->lock(s->context);
 ok=!take64(&s->fault)&&tsr_checkpoint_locked(&s->handoff,c,2,receipt);
 s->unlock(s->context);
 if(!ok)return transition_fail(s);
 if(c)return wait64(s,&s->active_generation,2)?true:transition_fail(s);
 if(!wait64(s,&s->handoff.arrived[1],2))return transition_fail(s);
 s->lock(s->context);
 ok=!take64(&s->fault)&&bank_live(&s->banks[1])&&nsrr_init_successor_locked(r,&s->handoff);
 s->unlock(s->context);
 return ok?true:transition_fail(s);
}
bool nsrc_run(struct nested_source_replace_chain *s,unsigned c,uint64_t generation){
 if(!s||s->self!=s||c>1||generation<1||generation>2||s->current_cpu(s->context)!=c||!healthy(s))return false;
 uint64_t active=take64(&s->active_generation);
 if((generation==1&&active!=1)||(generation==2&&active!=1))return false;
 if(generation==2&&!successor(s,c))return false;
 struct nested_source_replace_round *r=s->rounds[generation-1];
 if(!nsrr_run(r,c))return false;
 if(generation==1){
  /* This instruction is sequenced AFTER actual nsrr_run and nosr_commit
   * return. Publication below is the final access to old round payload. */
  struct tsr_ticket t=r->ticket;
  s->lock(s->context);bool ok=tsr_departed_locked(&s->handoff,c,t);s->unlock(s->context);
  return ok?true:transition_fail(s);
 }
 give64(&s->completed[c],2);return true;
}
bool nsrc_before_entry(struct nested_source_replace_chain *s,unsigned c,uint64_t generation){
 if(!s||s->self!=s||c>1||generation<1||generation>2||s->current_cpu(s->context)!=c||take64(&s->active_generation)!=generation||
    take64(&s->fault)||(generation==1&&take64(&s->transition[c])))return false;
 return nsrr_before_entry(s->rounds[generation-1],c);
}
