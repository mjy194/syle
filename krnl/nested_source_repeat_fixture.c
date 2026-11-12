#include "nested_source_repeat_fixture.h"
#include "paging.h"
#include "vmx_memory.h"
_Static_assert(EPT_GUEST_PAGES==63,"N48B retained fixture shape");
static uint64_t *view(uint64_t h){return paging_phys_to_virt(h);}
static bool repeat_authority(void *,const struct nrc_span *,uint64_t,struct nrc_atomic_page *);
static bool same(struct nrc_identity a,struct nrc_identity b){return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;}
static bool table(unsigned n){return (n>=16&&n<=20)||(n>=24&&n<=27);}
static unsigned permissions(unsigned n){return n==6?5:table(n)?1:3;}
static bool host(uint64_t h){return h&&!(h&4095)&&h<UINT64_C(0x100000000)&&view(h)&&paging_page_rw_wb(h)&&vmx_memory_region_wb(h);}
static const uint64_t *physical(const struct nested_domain_memory *d,unsigned r){return r==0?d->backing.tables:(r&1?d->root01[(r-1)/2]:d->root02[(r-1)/2]);}
static bool inventory(const struct nested_source_repeat_fixture *f){
 const struct nested_atomic_memory *m=&f->original;const struct nested_domain_memory *d=m->domain;
 uint64_t current[NAM_OWNED_PAGES];unsigned k=0;
 for(unsigned n=0;n<63;n++)current[k++]=d->backing.pages[n];
 for(unsigned n=0;n<4;n++)current[k++]=d->backing.tables[n];
 for(unsigned c=0;c<2;c++)for(unsigned kind=0;kind<2;kind++)for(unsigned n=0;n<4;n++)current[k++]=kind?d->root02[c][n]:d->root01[c][n];
 for(unsigned n=0;n<NAM_OWNED_PAGES;n++){
  if(current[n]!=m->owned[n]||!host(current[n]))return false;
  for(unsigned j=0;j<n;j++)if(current[n]==current[j])return false;
 }
 return d->backing.eptp==(d->backing.tables[0]|0x1e);
}
static bool maps(const struct nested_source_repeat_fixture *f){
 const struct nested_domain_memory *d=f->original.domain;
 /* Complete AD0 maps, including retained original and both parking roots.
  * No hidden leaf, huge mapping, alternate path or writable TABLE alias. */
 for(unsigned r=0;r<5;r++){
  const uint64_t *roots=physical(d,r);
  for(unsigned l=0;l<4;l++)for(unsigned n=0;n<512;n++){
   uint64_t want=l<3?(n?0:roots[l+1]|7):(n&&n<64?d->backing.pages[n-1]|0x70|permissions(n):0);
   if(__atomic_load_n(&view(roots[l])[n],__ATOMIC_ACQUIRE)!=want)return false;
  }
 }
 return true;
}
static bool stamp_same(struct ad_entry_stamp a,struct ad_entry_stamp b){
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.context_id==b.context_id&&a.epoch==b.epoch&&a.lifetime==b.lifetime;
}
static bool fresh(const void *p,size_t size){const unsigned char *b=p;for(size_t n=0;n<size;n++)if(b[n])return false;return true;}
static const struct nested_root_catalog *cat(const struct nested_source_repeat_fixture *f,unsigned bank){
 return !f||bank>2?NULL:bank?&f->banks[bank-1].catalog:&f->original.catalog;
}
struct nested_root_catalog *nested_source_repeat_catalog(struct nested_source_repeat_fixture *f,unsigned bank){
 return (struct nested_root_catalog *)cat(f,bank);
}
void *nested_source_repeat_context(struct nested_source_repeat_fixture *f,unsigned bank){
 return !f||bank>2?NULL:bank?(void *)&f->resolver[bank]:(void *)&f->original;
}
static uint64_t source_word(unsigned p,unsigned n,unsigned generation){
 if(p==16||p==20)return n?0:UINT64_C(0x11007);
 if(p==17||p==18||p==24||p==25||p==26)return n?0:((uint64_t)(p+1)<<12)|7;
 if((p==19||p==27)&&n&&n<64){
  unsigned mapped=n==15?(p==27?28:generation==1?29:15):n;
  return ((uint64_t)mapped<<12)|0x70|permissions(n);
 }return 0;
}
static bool sources(const struct nested_source_repeat_fixture *f,unsigned generation){
 for(unsigned p=16;p<=27;p++)if(table(p))for(unsigned n=0;n<512;n++){
  uint64_t want=source_word(p,n,generation),ad=want?(p==19||p==27?0x300:0x100):0;
  if((__atomic_load_n(&view(f->original.owned[p-1])[n],__ATOMIC_ACQUIRE)&~ad)!=want)return false;
 }return true;
}
static bool record(const struct nested_source_repeat_fixture *f,unsigned bank,const struct nrc_span *s,uint64_t g){
 if(bank>2||!g||(g&4095)||g>=UINT64_C(64)*4096)return false;
 const struct nrc_span *spans=bank?f->banks[bank-1].spans:f->original.spans;
 unsigned n=(unsigned)(g>>12);bool t=table(n);
 return s==&spans[n-1]&&same(s->identity,f->identities[bank])&&s->object==n&&
  s->lifetime==f->identities[0].lifetime&&s->gpa==g&&s->hpa==f->original.owned[n-1]&&
  s->bytes==4096&&s->table_view==(t?view(s->hpa):NULL)&&s->permissions==permissions(n)&&
  s->allowed_types==NRC_TYPE_WB&&s->role==(t?NRC_TABLE:NRC_DATA)&&s->ignore_pat;
}
static bool catalog_valid(const struct nested_source_repeat_fixture *f,unsigned bank){
 const struct nested_root_catalog *c=cat(f,bank);
 const struct nrc_span *spans=bank?f->banks[bank-1].spans:f->original.spans;
 const struct nrc_root *roots=bank?f->banks[bank-1].roots:f->original.roots;
 if(!c||!c->initialized||!same(c->identity,f->identities[bank])||c->spans!=spans||c->roots!=roots||
   c->span_count!=63||c->span_capacity!=63||c->root_count!=3||c->root_capacity!=3||
   !c->atomic_wb||c->atomic_uc||c->atomic_2m||c->atomic_1g||c->maxphys!=48||c->authority||
   c->atomic_authority!=(bank?repeat_authority:f->original_authority)||
   __atomic_load_n(&c->fault,__ATOMIC_ACQUIRE))return false;
 for(unsigned n=1;n<64;n++)if(!record(f,bank,&spans[n-1],(uint64_t)n<<12))return false;
 const uint64_t root_gpa[]={0x10000,0x14000,0x18000};
 for(unsigned r=0;r<3;r++)if(!same(roots[r].identity,f->identities[bank])||roots[r].source!=r+1||
  roots[r].owner!=f->identities[0].domain||roots[r].generation!=bank+1||roots[r].lifetime!=f->identities[0].lifetime||
  roots[r].full_eptp!=(root_gpa[r]|0x5e)||roots[r].root_gpa!=root_gpa[r])return false;
 return !bank||c->predecessor==cat(f,bank-1);
}
static bool identities_valid(const struct nested_source_repeat_fixture *f){
 for(unsigned bank=0;bank<3;bank++){
  struct nrc_identity id=f->identities[bank];
  struct ad_entry_stamp expected={id.domain,id.incarnation,id.catalog,bank+1,id.lifetime};
  if(!id.domain||!id.incarnation||!id.catalog||!id.lifetime||id.domain!=f->identities[0].domain||
   id.incarnation!=f->identities[0].incarnation||!stamp_same(expected,f->stamps[bank])||
   f->resolver[bank].fixture!=f||f->resolver[bank].bank!=bank)return false;
  for(unsigned prior=0;prior<bank;prior++)if(id.catalog==f->identities[prior].catalog||id.lifetime==f->identities[prior].lifetime)return false;
 }return true;
}
static bool structure(const struct nested_source_repeat_fixture *f,unsigned generation){
 if(!f||f->self!=f||!f->prepared||generation>2||!identities_valid(f))return false;
 const struct nested_atomic_memory *m=&f->original;const struct nested_domain_memory *d=m->domain;
 return m->prepared&&same(m->identity,f->identities[0])&&!m->large_2m&&!m->large_1g&&!m->fixed_uc&&
  d&&d->ready&&d->backing.started&&d->atomic_ad_prepared&&d->atomic_catalog_retained&&
  !d->contexts_prepared&&!d->eviction_prepared&&!d->typed_uc_prepared&&!d->typed_contexts_prepared&&
  !d->typed_dynamic_prepared&&!d->replacement_hpa&&!d->retained_hpa&&inventory(f)&&maps(f)&&sources(f,generation);
}
bool nested_source_repeat_ready(const struct nested_source_repeat_fixture *f,unsigned generation){
 if(!f||generation>2||__atomic_load_n(&f->published_generation,__ATOMIC_ACQUIRE)!=generation||
  __atomic_load_n(&f->structural_generation,__ATOMIC_ACQUIRE)!=generation||!structure(f,generation))return false;
 for(unsigned bank=0;bank<=generation;bank++){
  const struct nested_root_catalog *c=cat(f,bank);const struct ad_updater_gate *g=&f->gates[bank];
  if(!catalog_valid(f,bank)||!c->published||!g->initialized||!stamp_same(g->stamp,f->stamps[bank])||
   __atomic_load_n(&g->fault,__ATOMIC_ACQUIRE)||(bank&&g->predecessor!=&f->gates[bank-1]))return false;
  /* Only terminal predecessors are read here. Live nonce/reference fields
   * remain protected by their APIs and never leak into a resolver audit. */
  if(bank<generation&&(!c->closed||!c->superseded||!g->closed||!g->superseded))return false;
 }return true;
}
static bool repeat_authority(void *ctx,const struct nrc_span *s,uint64_t g,struct nrc_atomic_page *out){
 const struct nsrf_repeat_context *a=ctx;
 if(!a||!a->fixture||a->bank<1||a->bank>2)return false;
 struct nested_source_repeat_fixture *f=a->fixture;
 if(a!=&f->resolver[a->bank]||!out||!nested_source_repeat_ready(f,a->bank)||!record(f,a->bank,s,g))return false;
 bool t=s->role==NRC_TABLE;
 *out=(struct nrc_atomic_page){.backing={s->object,s->lifetime,s->hpa,s->table_view,s->permissions,6,true},
  .atomic_view=t?view(s->hpa):NULL,.pinned_writable=t};return true;
}
bool nested_source_repeat_prepare(struct nested_source_repeat_fixture *f,struct nested_domain_memory *d,const struct nrc_identity ids[3]){
 if(!f||!d||!ids||!fresh(f,sizeof *f))return false;
 for(unsigned n=0;n<3;n++){
  if(!ids[n].domain||!ids[n].incarnation||!ids[n].catalog||!ids[n].lifetime||
   ids[n].domain!=ids[0].domain||ids[n].incarnation!=ids[0].incarnation)return false;
  for(unsigned j=0;j<n;j++)if(ids[n].catalog==ids[j].catalog||ids[n].lifetime==ids[j].lifetime)return false;
 }
 f->self=f;
 for(unsigned n=0;n<3;n++){
  f->identities[n]=ids[n];f->resolver[n]=(struct nsrf_repeat_context){f,n};
  f->stamps[n]=(struct ad_entry_stamp){ids[n].domain,ids[n].incarnation,ids[n].catalog,n+1,ids[n].lifetime};
 }
 for(unsigned n=0;n<2;n++){f->steps[n].self=&f->steps[n];f->steps[n].fixture=f;f->steps[n].index=n;}
 if(!nested_atomic_memory_prepare(&f->original,d,ids[0])||!ad_updater_gate_init(&f->gates[0],f->stamps[0]))return false;
 f->original_authority=f->original.catalog.atomic_authority;
 for(unsigned n=0;n<512;n++)__atomic_store_n(&view(f->original.owned[28])[n],n?0:NSRF_REPEAT_Q_SENTINEL,__ATOMIC_RELAXED);
 f->prepared=true;return nested_source_repeat_ready(f,0);
}
static bool management(struct nsrf_repeat_step *s,uint64_t nonce){
 if(!s||s->self!=s||!s->fixture||s->index>1)return false;
 struct nested_source_repeat_fixture *f=s->fixture;
 return s==&f->steps[s->index]&&s->attempted&&!s->changed&&!s->published&&nonce&&nonce==s->ticket.nonce&&
  same(s->ticket.source,f->identities[s->index])&&stamp_same(s->ticket.stamp,f->stamps[s->index])&&
  s->guard&&s->guard(s->context,s->index,s->ticket)&&nested_source_repeat_ready(f,s->index)&&
  nrc_drained(nested_source_repeat_catalog(f,s->index),f->identities[s->index])&&
  ad_updater_gate_drained(&f->gates[s->index],f->stamps[s->index]);
}
static bool management_authority(void *ctx,uint64_t ticket,const struct nrc_span *span,uint64_t g,struct nsrm_authority_page *out){
 struct nsrf_repeat_step *s=ctx;
 if(!out||!management(s,ticket)||!record(s->fixture,s->index,span,g))return false;
 bool t=span->role==NRC_TABLE;uint64_t destination=s->index?NSRF_REPEAT_P:NSRF_REPEAT_Q;
 *out=(struct nsrm_authority_page){.identity=s->ticket.source,.ticket=ticket,.gpa=g,.object=span->object,
  .lifetime=span->lifetime,.hpa=span->hpa,.atomic_view=t?view(span->hpa):NULL,.permissions=span->permissions,
  .memory_type=6,.ignore_pat=true,.pinned=true,.guest_aliases_ro=t,.updater_drained=true,
  .replacement_data_safe=!t&&g==destination};return true;
}
static const unsigned data_pages[]={14,28,27};
bool nested_source_repeat_raw_unchanged(const struct nested_source_repeat_fixture *f,unsigned round){
 if(!f||f->self!=f||round>1)return false;
 const struct nsrf_repeat_step *s=&f->steps[round];const struct nsrm_plan *p=&s->plan;
 if(s->self!=s||!s->changed||p->self!=p||!p->started||!p->applied)return false;
 for(size_t page=0;page<p->page_count;page++)for(unsigned n=0;n<512;n++){
  uint64_t want=page==p->target&&n==p->request.index?p->request.new_leaf:p->pages[page].words[n];
  if(__atomic_load_n(&p->pages[page].view[n],__ATOMIC_ACQUIRE)!=want)return false;
 }
 for(unsigned page=0;page<3;page++)for(unsigned n=0;n<512;n++)
  if(__atomic_load_n(&view(f->original.owned[data_pages[page]])[n],__ATOMIC_ACQUIRE)!=s->data_before[page][n])return false;
 return true;
}
bool nested_source_repeat_change(struct nested_source_repeat_fixture *f,unsigned round,struct tsr_ticket ticket,
 nsrf_repeat_guard_fn guard,void *context){
 if(!f||round>1||!guard||!ticket.nonce||!nested_source_repeat_ready(f,round)||
  !same(ticket.source,f->identities[round])||!stamp_same(ticket.stamp,f->stamps[round])||
  ticket.domain.domain_id!=ticket.source.domain||ticket.domain.incarnation!=ticket.source.incarnation||
  !ticket.domain.epoch||ticket.domain.purpose!=NESTED_DOMAIN_SOURCE_REPLACE)return false;
 struct nsrf_repeat_step *s=&f->steps[round];struct nsrf_repeat_bank *b=&f->banks[round];
 struct nested_root_catalog *old=nested_source_repeat_catalog(f,round);unsigned next=round+1;
 uint64_t catalog_nonce,updater_nonce;
 if(s->self!=s||s->fixture!=f||s->index!=round||s->attempted||s->changed||s->published||
  !fresh(&s->plan,sizeof s->plan)||!fresh(b,sizeof *b)||!fresh(&f->gates[next],sizeof f->gates[next])||
  !nrc_successor_budget(old,f->identities[round],NSRF_REPEAT_NONCES,&catalog_nonce)||
  !ad_updater_gate_successor_budget(&f->gates[round],f->stamps[round],NSRF_REPEAT_NONCES,&updater_nonce)||
  !guard(context,round,ticket))return false;
 for(size_t n=0;n<old->root_count;n++)if(old->roots[n].generation==UINT64_MAX)return false;
 /* All predictable current-bank budget/identity/storage failures precede
  * the one-shot marker and CAS. This snapshot does not reserve resources:
  * the exact full stopped/drained guard remains mandatory through apply. */
 s->ticket=ticket;s->guard=guard;s->context=context;s->catalog_nonce=catalog_nonce;s->updater_nonce=updater_nonce;s->attempted=true;
 for(unsigned page=0;page<3;page++)for(unsigned n=0;n<512;n++)s->data_before[page][n]=__atomic_load_n(&view(f->original.owned[data_pages[page]])[n],__ATOMIC_ACQUIRE);
 uint64_t destination=round?NSRF_REPEAT_P:NSRF_REPEAT_Q;unsigned page=round?14:28;
 struct nsrm_request req={.ticket=ticket.nonce,.full_eptp=0x1005e,.l2_gpa=NSRF_REPEAT_P,
  .table_gpa=0x13000,.table_hpa=f->original.owned[18],.table_object=19,.table_lifetime=f->identities[0].lifetime,
  .expected_old=__atomic_load_n(&view(f->original.owned[18])[15],__ATOMIC_ACQUIRE),
  .new_leaf=destination|0x73,.new_gpa=destination,.new_hpa=f->original.owned[page],
  .new_object=page+1,.new_lifetime=f->identities[0].lifetime,.index=15};
 if(!nsrm_prepare(&s->plan,old,management_authority,s,req)||!management(s,ticket.nonce)||!nsrm_apply(&s->plan))return false;
 s->changed=true;__atomic_store_n(&f->structural_generation,next,__ATOMIC_RELEASE);
 if(!nested_source_repeat_raw_unchanged(f,round)||!structure(f,next)||!guard(context,round,ticket)||
  !nrc_init_atomic_wb_successor(&b->catalog,old,f->identities[round],f->identities[next],48,b->spans,63,b->roots,3,repeat_authority,NSRF_REPEAT_NONCES)||
  !ad_updater_gate_init_successor(&f->gates[next],&f->gates[round],f->stamps[round],f->stamps[next],NSRF_REPEAT_NONCES))return false;
 /* New banks are still exclusive/unpublished: compare captured high-water,
  * then never read live nonce fields from reader-side authority callbacks. */
 if(b->catalog.next_nonce!=s->catalog_nonce||f->gates[next].next_nonce!=s->updater_nonce)return false;
 for(unsigned n=0;n<63;n++){struct nrc_span span=old->spans[n];span.identity=f->identities[next];
  if(!nrc_add_atomic_span(&b->catalog,span,span.role==NRC_TABLE?view(span.hpa):NULL))return false;}
 for(unsigned n=0;n<3;n++){struct nrc_root root=old->roots[n];root.identity=f->identities[next];root.generation++;
  if(!nrc_add_root(&b->catalog,root))return false;}
 if(!catalog_valid(f,next)||!nested_source_repeat_raw_unchanged(f,round)||!guard(context,round,ticket)||!nrc_publish(&b->catalog))return false;
 s->published=true;__atomic_store_n(&f->published_generation,next,__ATOMIC_RELEASE);
 return nested_source_repeat_ready(f,next);
}
