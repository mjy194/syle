#include "nested_atomic_memory.h"
#include "paging.h"
#include "memory.h"
#include "vmx_memory.h"
_Static_assert(EPT_GUEST_PAGES==63,"N41C fixture shape");
static uint64_t *view(uint64_t h){return paging_phys_to_virt(h);}
static bool id_eq(struct nrc_identity a,struct nrc_identity b){return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;}
static bool table_page(unsigned n,bool giga){return (n>=16&&n<=20)||(n>=24&&n<=27)||(giga&&(n==21||n==22));}
static unsigned permissions(unsigned n,bool final,bool giga){return n==6?5:((final?table_page(n,giga):(n>=16&&n<=19))?1:3);}
static void inventory(const struct nested_domain_memory *d,uint64_t *v){
 unsigned k=0;
 for(unsigned n=0;n<63;n++)v[k++]=d->backing.pages[n];
 for(unsigned l=0;l<4;l++)v[k++]=d->backing.tables[l];
 for(unsigned c=0;c<2;c++)for(unsigned kind=0;kind<2;kind++)for(unsigned l=0;l<4;l++)v[k++]=kind?d->root02[c][l]:d->root01[c][l];
}
static bool host(uint64_t h){return h&&!(h&4095)&&h<UINT64_C(0x100000000)&&view(h)&&paging_page_rw_wb(h)&&vmx_memory_region_wb(h);}
static bool owned_policy(const struct nested_atomic_memory *m,bool snapshot,bool uc,nam_uc_guard_fn guard,void *context){
 uint64_t v[NAM_OWNED_PAGES];inventory(m->domain,v);
 for(unsigned i=0;i<NAM_OWNED_PAGES;i++){
  bool valid=uc&&i==14?(v[i]&&!(v[i]&4095)&&v[i]<UINT64_C(0x100000000)&&view(v[i])&&guard&&guard(context,v[i],m->type_epoch)):host(v[i]);
  if(!valid||(snapshot&&v[i]!=m->owned[i]))return false;
  for(unsigned j=0;j<i;j++)if(v[i]==v[j])return false;
 }
 if(m->large_1g&&snapshot)for(unsigned i=0;i<10;i++){
  if(!host(m->sparse_tables[i]))return false;
  for(unsigned j=0;j<NAM_OWNED_PAGES;j++)if(m->sparse_tables[i]==v[j])return false;
  for(unsigned j=0;j<i;j++)if(m->sparse_tables[i]==m->sparse_tables[j])return false;
 }
 return m->domain->backing.eptp==(m->domain->backing.tables[0]|0x1e);
}
static bool owned(const struct nested_atomic_memory *m,bool snapshot){
 return owned_policy(m,snapshot,m->fixed_uc&&m->uc_installed,m->uc_guard,m->uc_context);
}
/* AD0 EPT01 and retained prebuilt maps never acquire hardware A/D. Full
 * exact paths rule out hidden writable aliases, huge leaves and spare paths. */
static bool physical_maps(const struct nested_atomic_memory *m,bool final){
 const struct nested_domain_memory *d=m->domain;
 for(unsigned r=0;r<5;r++){
  const uint64_t *roots=r==0?d->backing.tables:(r&1?d->root01[(r-1)/2]:d->root02[(r-1)/2]);
  for(unsigned l=0;l<4;l++){
   const uint64_t *v=view(roots[l]);if(!v)return false;
   for(unsigned n=0;n<512;n++){
    unsigned p=final?permissions(n,true,m->large_1g):(r==0?(n==6?5:3):permissions(n,false,false));
    uint64_t want=l<3?(n?0:roots[l+1]|7):(n&&n<64?d->backing.pages[n-1]|(final?(m->fixed_uc&&n==15?0x40:0x70):0x30)|p:0);
    if(!final&&m->uc_detached&&l==3&&n==15)want=0;
    if(final&&m->large_1g&&l==2&&(n==1||n==511))want=m->sparse_tables[r*2+(n==511)]|7;
    if(__atomic_load_n(&v[n],__ATOMIC_ACQUIRE)!=want)return false;
   }
  }
  if(final&&m->large_1g)for(unsigned t=0;t<2;t++)for(unsigned n=0;n<512;n++){
   uint64_t want=n==(t?511u:0u)?d->backing.pages[t?30:28]|0x73:0;
   if(__atomic_load_n(&view(m->sparse_tables[r*2+t])[n],__ATOMIC_ACQUIRE)!=want)return false;
  }
 }
 return true;
}
static uint64_t source_word(unsigned page,unsigned n,bool final,bool large,bool giga,bool uc){
 if(final&&giga){
  if(page==17&&n==1)return 0xf7;
  if(page==25&&n==1)return 0x1a007;
  if(page==26&&n==1)return 0x15007;
  if(page==26&&n==511)return 0x16007;
  if(page==21)return n==0?0x200073:0;
  if(page==22)return n==511?0x3ffff073:0;
 }
 if(final&&large&&n==1){
  if(page==18)return UINT64_C(0xf7); /* WB/IPAT1, PS, base zero. */
  if(page==26)return UINT64_C(0x1b007); /* C retains alternate 4KiB mapping. */
 }
 if(page==16||page==20)return n?0:UINT64_C(0x11007);
 if(page==17||page==18||page==24||page==25||page==26)return n?0:((uint64_t)(page+1)<<12)|7;
 if((page==19||page==27)&&n&&n<64){
  if(final&&giga&&(n==29||n==31))return (n==29?UINT64_C(0x200000):UINT64_C(0x3ffff000))|0x73;
  unsigned mapped=page==27&&n==15?28:n;
  return ((uint64_t)mapped<<12)|(final?(uc&&mapped==15?0x40:0x70):0x30)|permissions(n,final,giga);
 }
 return 0;
}
static bool sources(const struct nested_atomic_memory *m,bool final){
 for(unsigned p=16;p<=27;p++){
  if(final?!table_page(p,m->large_1g):p>19)continue;
  const uint64_t *v=view(m->domain->backing.pages[p-1]);if(!v)return false;
  for(unsigned n=0;n<512;n++){
   uint64_t want=source_word(p,n,final,m->large_2m,m->large_1g,m->fixed_uc);
   uint64_t got=__atomic_load_n(&v[n],__ATOMIC_ACQUIRE);
   uint64_t ad=final&&want?(p==19||p==27||(p==18&&m->large_2m&&n==1)||(m->large_1g&&((p==17&&n==1)||p==21||p==22))?0x300:0x100):0;
   if((got&~ad)!=want)return false;
  }
 }
 return true;
}
static bool flags(const struct nested_domain_memory *d,bool final){
 return d&&d->ready&&d->backing.started&&d->atomic_ad_prepared==final&&d->atomic_catalog_retained==final&&!d->contexts_prepared&&!d->eviction_prepared&&
  !d->typed_uc_prepared&&!d->typed_contexts_prepared&&!d->typed_dynamic_prepared&&!d->replacement_hpa&&!d->retained_hpa;
}
bool nested_atomic_memory_ready(const struct nested_atomic_memory *m){
 return m&&m->prepared&&flags(m->domain,true)&&owned(m,true)&&physical_maps(m,true)&&sources(m,true);
}
static bool authority(void *context,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *out){
 struct nested_atomic_memory *m=context;
 if(!out||!m||!m->prepared||!s||!gpa||(gpa&4095))return false;
 unsigned n=m->large_1g&&gpa==0x200000?29:m->large_1g&&gpa==0x3ffff000?31:gpa>>12;
 if(n<1||n>63||(m->large_1g&&((n==29&&gpa!=0x200000)||(n==31&&gpa!=0x3ffff000))))return false;
 bool table=table_page(n,m->large_1g);
 /* Check exact registered storage as well as independently derived values.
  * Caller-supplied lookalikes cannot gain authority for another catalog. */
 if(s!=&m->spans[n-1]||!id_eq(s->identity,m->identity)||s->object!=n||s->lifetime!=m->identity.lifetime||
  s->gpa!=gpa||s->hpa!=m->owned[n-1]||s->bytes!=4096||s->permissions!=permissions(n,true,m->large_1g)||
  s->allowed_types!=(m->fixed_uc&&n==15?NRC_TYPE_UC:NRC_TYPE_WB)||!s->ignore_pat||s->role!=(table?NRC_TABLE:NRC_DATA)||
  s->table_view!=(table?view(m->owned[n-1]):NULL)||!nested_atomic_memory_ready(m))return false;
 *out=(struct nrc_atomic_page){.backing={.object=s->object,.lifetime=s->lifetime,.hpa=s->hpa,
  .table_view=s->table_view,.permissions=s->permissions,.memory_type=m->fixed_uc&&n==15?0:6,.ignore_pat=true},
  .atomic_view=table?view(s->hpa):NULL,.pinned_writable=table};
 return true;
}
static bool prepare(struct nested_atomic_memory *m,struct nested_domain_memory *d,struct nrc_identity id,bool large,bool giga,uint64_t uc_epoch){
 if(!m||m->prepared||m->catalog.initialized||!id.domain||!id.incarnation||!id.catalog||!id.lifetime||!flags(d,false)||!d->backing.started)return false;
 /* Fresh-object-only constructor. Validation below has no backing writes. */
 *m=(struct nested_atomic_memory){.identity=id,.domain=d,.large_2m=large,.large_1g=giga,.fixed_uc=uc_epoch!=0,.type_epoch=uc_epoch};
 if(!owned(m,false)||!physical_maps(m,false)||!sources(m,false))return false;
 if(large||giga)for(unsigned c=0;c<2;c++)if(view(d->backing.pages[c*32+3])[15]!=0xf063||view(d->backing.pages[c*32+3])[48]!=0x30063||(giga&&view(d->backing.pages[c*32+3])[49]!=0x31063))return false;
 inventory(d,m->owned);
 if(giga){
  for(unsigned i=0;i<10;i++){
   m->sparse_tables[i]=pmm_alloc_page_below(UINT64_C(0x100000000));
   if(!host(m->sparse_tables[i]))return false;
   for(unsigned j=0;j<NAM_OWNED_PAGES;j++)if(m->sparse_tables[i]==m->owned[j])return false;
   for(unsigned j=0;j<i;j++)if(m->sparse_tables[i]==m->sparse_tables[j])return false;
  }
  for(unsigned i=0;i<10;i++)for(unsigned n=0;n<512;n++)view(m->sparse_tables[i])[n]=0;
 }
 if(!(uc_epoch?nrc_init_atomic_uc(&m->catalog,id,48,m->spans,63,m->roots,3,authority,0xf000,m->owned[14],uc_epoch):giga?nrc_init_atomic_wb_1g(&m->catalog,id,48,m->spans,63,m->roots,3,authority):large?nrc_init_atomic_wb_2m(&m->catalog,id,48,m->spans,63,m->roots,3,authority):
   nrc_init_atomic_wb(&m->catalog,id,48,m->spans,63,m->roots,3,authority)))return false;
 for(unsigned n=1;n<64;n++){
  bool t=table_page(n,giga);
  struct nrc_span s={.identity=id,.object=n,.lifetime=id.lifetime,.gpa=giga&&n==29?0x200000:giga&&n==31?0x3ffff000:(uint64_t)n<<12,
   .hpa=m->owned[n-1],.bytes=4096,.table_view=t?view(m->owned[n-1]):NULL,
   .permissions=permissions(n,true,m->large_1g),.allowed_types=uc_epoch&&n==15?NRC_TYPE_UC:NRC_TYPE_WB,.role=t?NRC_TABLE:NRC_DATA,.ignore_pat=true};
  if(!nrc_add_atomic_span(&m->catalog,s,t?view(s.hpa):NULL))return false;
 }
 const uint64_t roots[]={0x10000,0x14000,0x18000};
 for(unsigned r=0;r<3;r++)if(!nrc_add_root(&m->catalog,(struct nrc_root){.identity=id,.source=r+1,.owner=id.domain,
   .generation=1,.lifetime=id.lifetime,.full_eptp=roots[r]|0x5e,.root_gpa=roots[r]}))return false;
 if(uc_epoch){
  /* No physical/source writes. Retention is irrevocable before later detach. */
  d->atomic_catalog_retained=true;m->uc_staged=true;return true;
 }
 /* Complete old shape and catalog registration validated before first write.
  * Every subsequent backing store is infallible on proven owned mapped RAM. */
 for(unsigned p=16;p<=27;p++)if(table_page(p,m->large_1g))for(unsigned n=0;n<512;n++)
  __atomic_store_n(&view(d->backing.pages[p-1])[n],source_word(p,n,true,large,giga,false),__ATOMIC_RELAXED);
 for(unsigned r=0;r<5;r++){
  const uint64_t *roots4=r==0?d->backing.tables:(r&1?d->root01[(r-1)/2]:d->root02[(r-1)/2]);
  if(giga)for(unsigned t=0;t<2;t++){
   view(roots4[2])[t?511:1]=m->sparse_tables[r*2+t]|7;
   view(m->sparse_tables[r*2+t])[t?511:0]=d->backing.pages[t?30:28]|0x73;
  }
  for(unsigned n=1;n<64;n++)view(roots4[3])[n]=d->backing.pages[n-1]|0x70|permissions(n,true,m->large_1g);
 }
 if(large||giga)for(unsigned c=0;c<2;c++){
  view(d->backing.pages[c*32+3])[15]=(giga?UINT64_C(0x4000f063):UINT64_C(0x20f063));
  if(giga)view(d->backing.pages[c*32+3])[49]=UINT64_C(0x7ffff063);
  view(d->backing.pages[c*32+3])[48]=(giga?UINT64_C(0x40200063):UINT64_C(0x21d063));
 }
 view(d->backing.pages[27])[0]=UINT64_C(0x99aa99aa99aa99aa);
 d->atomic_ad_prepared=true;d->atomic_catalog_retained=true;
 m->prepared=true;
 if(!nrc_publish(&m->catalog)){m->prepared=false;return false;}
 return true;
}

bool nested_atomic_memory_prepare(struct nested_atomic_memory *m,struct nested_domain_memory *d,struct nrc_identity id){return prepare(m,d,id,false,false,0);}
bool nested_atomic_memory_prepare_2m(struct nested_atomic_memory *m,struct nested_domain_memory *d,struct nrc_identity id){return prepare(m,d,id,true,false,0);}

bool nested_atomic_memory_prepare_1g(struct nested_atomic_memory *m,struct nested_domain_memory *d,struct nrc_identity id){return prepare(m,d,id,false,true,0);}

/* Staged metadata is not authority for a source walk. No caller may clear
 * these retention flags or release this domain after stage succeeds. */
static bool uc_stage_flags(const struct nested_atomic_memory *m){
 const struct nested_domain_memory *d=m?m->domain:0;
 return m&&m->fixed_uc&&m->uc_staged&&!m->prepared&&m->type_epoch&&
  !m->large_2m&&!m->large_1g&&d&&d->ready&&d->backing.started&&d->atomic_catalog_retained&&
  !d->atomic_ad_prepared&&!d->contexts_prepared&&!d->eviction_prepared&&!d->typed_uc_prepared&&
  !d->typed_contexts_prepared&&!d->typed_dynamic_prepared&&!d->replacement_hpa&&!d->retained_hpa&&
  m->catalog.initialized&&m->catalog.atomic_uc&&!m->catalog.published&&!m->catalog.closed&&!m->catalog.fault&&
  !m->catalog.references&&!m->catalog.sessions&&m->catalog.uc_gpa==0xf000&&m->catalog.uc_hpa==m->owned[14]&&
  m->catalog.type_epoch==m->type_epoch;
}
bool nested_atomic_memory_uc_stage(struct nested_atomic_memory *m,struct nested_domain_memory *d,struct nrc_identity id,uint64_t epoch){
 return epoch&&prepare(m,d,id,false,false,epoch);
}
bool nested_atomic_memory_uc_staged(const struct nested_atomic_memory *m){
 return uc_stage_flags(m)&&!m->uc_detached&&!m->uc_installed&&owned(m,true)&&physical_maps(m,false)&&sources(m,false);
}
bool nested_atomic_memory_uc_detach(struct nested_atomic_memory *m){
 if(!nested_atomic_memory_uc_staged(m))return false;
 for(unsigned r=0;r<5;r++){
  struct nested_domain_memory *d=m->domain;
  const uint64_t *roots=r==0?d->backing.tables:(r&1?d->root01[(r-1)/2]:d->root02[(r-1)/2]);
  __atomic_store_n(&view(roots[3])[15],0,__ATOMIC_RELEASE);
 }
 m->uc_detached=true;return true;
}
bool nested_atomic_memory_uc_install(struct nested_atomic_memory *m,nam_uc_guard_fn guard,void *context){
 if(!uc_stage_flags(m)||!m->uc_detached||m->uc_installed||!guard||
    !owned_policy(m,true,true,guard,context)||!physical_maps(m,false)||!sources(m,false))return false;
 struct nested_domain_memory *d=m->domain;
 /* All final stores are to proven owned WB tables/ordinary WB alternate data.
  * The UC target itself is never dereferenced by this adapter. */
 for(unsigned p=16;p<=27;p++)if(table_page(p,false))for(unsigned n=0;n<512;n++)
  __atomic_store_n(&view(d->backing.pages[p-1])[n],source_word(p,n,true,false,false,true),__ATOMIC_RELAXED);
 for(unsigned r=0;r<5;r++){
  const uint64_t *roots=r==0?d->backing.tables:(r&1?d->root01[(r-1)/2]:d->root02[(r-1)/2]);
  for(unsigned n=1;n<64;n++)__atomic_store_n(&view(roots[3])[n],
   d->backing.pages[n-1]|(n==15?0x40:0x70)|permissions(n,true,false),__ATOMIC_RELEASE);
 }
 view(d->backing.pages[27])[0]=UINT64_C(0x99aa99aa99aa99aa);
 m->uc_guard=guard;m->uc_context=context;m->uc_installed=true;return true;
}
bool nested_atomic_memory_uc_installed(const struct nested_atomic_memory *m){
 return uc_stage_flags(m)&&m->uc_detached&&m->uc_installed&&owned(m,true)&&physical_maps(m,true)&&sources(m,true);
}
bool nested_atomic_memory_uc_publish(struct nested_atomic_memory *m){
 if(!nested_atomic_memory_uc_installed(m))return false;
 m->domain->atomic_ad_prepared=true;m->prepared=true;
 if(!nrc_publish(&m->catalog)){m->prepared=false;return false;}
 return nested_atomic_memory_ready(m);
}
