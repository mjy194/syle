#include "nested_source_replace_fixture.h"
#include "paging.h"
#include "vmx_memory.h"
_Static_assert(EPT_GUEST_PAGES==63,"N47B retained fixture shape");
static uint64_t *view(uint64_t h){return paging_phys_to_virt(h);}
static bool successor_authority(void *,const struct nrc_span *,uint64_t,struct nrc_atomic_page *);
static bool same(struct nrc_identity a,struct nrc_identity b){return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;}
static bool table(unsigned n){return (n>=16&&n<=20)||(n>=24&&n<=27);}
static unsigned permissions(unsigned n){return n==6?5:table(n)?1:3;}
static bool host(uint64_t h){return h&&!(h&4095)&&h<UINT64_C(0x100000000)&&view(h)&&paging_page_rw_wb(h)&&vmx_memory_region_wb(h);}
static const uint64_t *physical(const struct nested_domain_memory *d,unsigned r){return r==0?d->backing.tables:(r&1?d->root01[(r-1)/2]:d->root02[(r-1)/2]);}
static bool inventory(const struct nested_source_replace_fixture *f){
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
static bool maps(const struct nested_source_replace_fixture *f){
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
static uint64_t source_word(unsigned p,unsigned n,bool changed){
 if(p==16||p==20)return n?0:UINT64_C(0x11007);
 if(p==17||p==18||p==24||p==25||p==26)return n?0:((uint64_t)(p+1)<<12)|7;
 if((p==19||p==27)&&n&&n<64){
  unsigned mapped=n==15?(p==27?28:changed?29:15):n;
  return ((uint64_t)mapped<<12)|0x70|permissions(n);
 }
 return 0;
}
static bool sources(const struct nested_source_replace_fixture *f){
 for(unsigned p=16;p<=27;p++)if(table(p))for(unsigned n=0;n<512;n++){
  uint64_t want=source_word(p,n,f->changed);
  uint64_t ad=want?(p==19||p==27?0x300:0x100):0;
  if((__atomic_load_n(&view(f->original.owned[p-1])[n],__ATOMIC_ACQUIRE)&~ad)!=want)return false;
 }
 return true;
}
static bool record(const struct nested_source_replace_fixture *f,unsigned bank,const struct nrc_span *s,uint64_t g){
 const struct nrc_span *spans=bank?f->spans:f->original.spans;
 if(!g||(g&4095)||g>=UINT64_C(64)*4096)return false;
 unsigned n=(unsigned)(g>>12);bool t=table(n);
 return s==&spans[n-1]&&same(s->identity,f->identity[bank])&&s->object==n&&
  s->lifetime==f->identity[0].lifetime&&s->gpa==g&&s->hpa==f->original.owned[n-1]&&
  s->bytes==4096&&s->table_view==(t?view(s->hpa):NULL)&&s->permissions==permissions(n)&&
  s->allowed_types==NRC_TYPE_WB&&s->role==(t?NRC_TABLE:NRC_DATA)&&s->ignore_pat;
}
static bool catalog(const struct nested_source_replace_fixture *f,unsigned bank){
 const struct nested_root_catalog *c=bank?&f->successor:&f->original.catalog;
 const struct nrc_span *spans=bank?f->spans:f->original.spans;
 const struct nrc_root *roots=bank?f->roots:f->original.roots;
 if(!c->initialized||!same(c->identity,f->identity[bank])||c->spans!=spans||c->roots!=roots||
    c->span_count!=63||c->span_capacity!=63||c->root_count!=3||c->root_capacity!=3||
    !c->atomic_wb||c->atomic_uc||c->atomic_2m||c->atomic_1g||c->maxphys!=48||c->authority||
    c->atomic_authority!=(bank?successor_authority:f->original_authority)||
    __atomic_load_n(&c->fault,__ATOMIC_ACQUIRE))return false;
 for(unsigned n=1;n<64;n++)if(!record(f,bank,&spans[n-1],(uint64_t)n<<12))return false;
 const uint64_t root_gpa[]={0x10000,0x14000,0x18000};
 for(unsigned r=0;r<3;r++)if(!same(roots[r].identity,f->identity[bank])||roots[r].source!=r+1||
  roots[r].owner!=f->identity[0].domain||roots[r].generation!=bank+1||roots[r].lifetime!=f->identity[0].lifetime||
  roots[r].full_eptp!=(root_gpa[r]|0x5e)||roots[r].root_gpa!=root_gpa[r])return false;
 /* next_nonce/references/sessions are live catalog-lock fields. Authority
  * runs outside that lock and must never inspect the successor's copies. */
 if(bank&&(!f->original.catalog.superseded||c->predecessor!=&f->original.catalog||
    c->atomic_authority!=successor_authority))return false;
 return true;
}
bool nested_source_replace_fixture_ready(const struct nested_source_replace_fixture *f){
 if(!f||f->self!=f||!f->prepared)return false;
 const struct nested_atomic_memory *m=&f->original;const struct nested_domain_memory *d=m->domain;
 if(!m->prepared||!same(m->identity,f->identity[0])||m->large_2m||m->large_1g||m->fixed_uc||
    !d||!d->ready||!d->backing.started||!d->atomic_ad_prepared||!d->atomic_catalog_retained||
    d->contexts_prepared||d->eviction_prepared||d->typed_uc_prepared||d->typed_contexts_prepared||
    d->typed_dynamic_prepared||d->replacement_hpa||d->retained_hpa||!catalog(f,0)||
    (f->successor.initialized&&!catalog(f,1))||!inventory(f)||!maps(f)||!sources(f))return false;
 /* changed is published only after old close+drain, with no surviving old
  * session. That bank stays terminal and is never reopened or reset. */
 return f->original.catalog.published&&
  (!f->changed||(f->original.catalog.closed&&!f->original.catalog.references&&!f->original.catalog.sessions))&&
  (!f->published||(f->changed&&f->successor.published));
}
static bool successor_authority(void *ctx,const struct nrc_span *s,uint64_t g,struct nrc_atomic_page *out){
 struct nested_source_replace_fixture *f=ctx;
 if(!out||!f||!f->published||!f->changed||!record(f,1,s,g)||!nested_source_replace_fixture_ready(f))return false;
 bool t=s->role==NRC_TABLE;
 *out=(struct nrc_atomic_page){.backing={.object=s->object,.lifetime=s->lifetime,.hpa=s->hpa,
  .table_view=s->table_view,.permissions=s->permissions,.memory_type=6,.ignore_pat=true},
  .atomic_view=t?view(s->hpa):NULL,.pinned_writable=t};return true;
}
bool nested_source_replace_fixture_prepare(struct nested_source_replace_fixture *f,struct nested_domain_memory *d,
 struct nrc_identity old_id,struct nrc_identity new_id){
 if(!f||f->self||f->prepared||!new_id.domain||!new_id.incarnation||!new_id.catalog||!new_id.lifetime||
    new_id.domain!=old_id.domain||new_id.incarnation!=old_id.incarnation||
    new_id.catalog==old_id.catalog||new_id.lifetime==old_id.lifetime)return false;
 f->self=f;f->identity[0]=old_id;f->identity[1]=new_id;
 if(!nested_atomic_memory_prepare(&f->original,d,old_id))return false;
 f->original_authority=f->original.catalog.atomic_authority;
 /* GPA1d000 is outside both CPUs' paging/stack/code/VMCS payload bands;
  * distinctness from every fixed table/control page is proven by inventory. */
 for(unsigned n=0;n<512;n++)__atomic_store_n(&view(f->original.owned[28])[n],n?0:NSRF_NEW_SENTINEL,__ATOMIC_RELAXED);
 f->prepared=true;return nested_source_replace_fixture_ready(f);
}
static bool management(const struct nested_source_replace_fixture *f,uint64_t ticket){
 return f&&f->self==f&&f->attempted&&!f->changed&&!f->published&&ticket&&ticket==f->ticket&&
  f->management_guard&&f->management_guard(f->management_context,ticket)&&
  nrc_drained((struct nested_root_catalog *)&f->original.catalog,f->identity[0])&&nested_source_replace_fixture_ready(f);
}
bool nested_source_replace_fixture_authority(void *ctx,uint64_t ticket,const struct nrc_span *s,uint64_t g,struct nsrm_authority_page *out){
 struct nested_source_replace_fixture *f=ctx;
 if(!out||!f||!record(f,0,s,g)||!management(f,ticket))return false;
 bool t=s->role==NRC_TABLE;
 *out=(struct nsrm_authority_page){.identity=f->identity[0],.ticket=ticket,.gpa=g,.object=s->object,
  .lifetime=s->lifetime,.hpa=s->hpa,.atomic_view=t?view(s->hpa):NULL,.permissions=s->permissions,
  .memory_type=6,.ignore_pat=true,.pinned=true,.guest_aliases_ro=t,.updater_drained=true,
  .replacement_data_safe=!t&&g==NSRF_NEW_GPA};return true;
}
static const unsigned data_pages[]={14,28,27};
bool nested_source_replace_fixture_raw_unchanged(const struct nested_source_replace_fixture *f){
 if(!f||f->self!=f||!f->changed||f->plan.self!=&f->plan||!f->plan.started||!f->plan.applied)return false;
 for(size_t p=0;p<f->plan.page_count;p++)for(unsigned n=0;n<512;n++){
  uint64_t want=p==f->plan.target&&n==f->plan.request.index?f->plan.request.new_leaf:f->plan.pages[p].words[n];
  if(__atomic_load_n(&f->plan.pages[p].view[n],__ATOMIC_ACQUIRE)!=want)return false;
 }
 for(unsigned p=0;p<3;p++)for(unsigned n=0;n<512;n++)
  if(__atomic_load_n(&view(f->original.owned[data_pages[p]])[n],__ATOMIC_ACQUIRE)!=f->data_before[p][n])return false;
 return true;
}
bool nested_source_replace_fixture_change(struct nested_source_replace_fixture *f,uint64_t ticket,
 nsrf_management_guard_fn guard,void *ctx,uint64_t reserve){
 if(!f||f->self!=f||!f->prepared||f->attempted||!ticket||!guard||!reserve||
    !nested_source_replace_fixture_ready(f)||!nrc_drained(&f->original.catalog,f->identity[0])||
    reserve>UINT64_MAX-f->original.catalog.next_nonce||!guard(ctx,ticket))return false;
 f->attempted=true;f->ticket=ticket;f->management_guard=guard;f->management_context=ctx;
 for(unsigned p=0;p<3;p++)for(unsigned n=0;n<512;n++)f->data_before[p][n]=__atomic_load_n(&view(f->original.owned[data_pages[p]])[n],__ATOMIC_ACQUIRE);
 struct nsrm_request request={.ticket=ticket,.full_eptp=0x1005e,.l2_gpa=NSRF_TARGET_GPA,
  .table_gpa=0x13000,.table_hpa=f->original.owned[18],.table_object=19,.table_lifetime=f->identity[0].lifetime,
  .expected_old=__atomic_load_n(&view(f->original.owned[18])[15],__ATOMIC_ACQUIRE),
  .new_leaf=NSRF_NEW_GPA|0x73,.new_gpa=NSRF_NEW_GPA,.new_hpa=f->original.owned[28],
  .new_object=29,.new_lifetime=f->identity[0].lifetime,.index=15};
 if(!nsrm_prepare(&f->plan,&f->original.catalog,nested_source_replace_fixture_authority,f,request)||!nsrm_apply(&f->plan))return false;
 f->changed=true;
 if(!nested_source_replace_fixture_raw_unchanged(f)||!guard(ctx,ticket)||!nested_source_replace_fixture_ready(f)||
    !nrc_init_atomic_wb_successor(&f->successor,&f->original.catalog,f->identity[0],f->identity[1],48,
      f->spans,63,f->roots,3,successor_authority,reserve))return false;
 /* Exclusive and unpublished here: the inherited nonce is checked once.
  * Subsequent nrc_begin calls own this mutable field under the catalog lock. */
 if(f->successor.next_nonce!=f->original.catalog.next_nonce)return false;
 for(unsigned n=0;n<63;n++){
  struct nrc_span s=f->original.spans[n];s.identity=f->identity[1];
  if(!nrc_add_atomic_span(&f->successor,s,s.role==NRC_TABLE?view(s.hpa):NULL))return false;
 }
 for(unsigned n=0;n<3;n++){
  struct nrc_root r=f->original.roots[n];r.identity=f->identity[1];r.generation++;
  if(!nrc_add_root(&f->successor,r))return false;
 }
 if(!nested_source_replace_fixture_ready(f)||!nested_source_replace_fixture_raw_unchanged(f)||!guard(ctx,ticket)||!nrc_publish(&f->successor))return false;
 f->published=true;return nested_source_replace_fixture_ready(f);
}
