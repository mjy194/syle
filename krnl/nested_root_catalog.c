#include "nested_root_catalog.h"
static bool identity(struct nrc_identity a,struct nrc_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;
}
static bool root_equal(struct nrc_root a,struct nrc_root b) {
 return identity(a.identity,b.identity)&&a.source==b.source&&a.owner==b.owner&&
  a.generation==b.generation&&a.lifetime==b.lifetime&&a.full_eptp==b.full_eptp&&a.root_gpa==b.root_gpa;
}
static bool id_valid(struct nrc_identity a){return a.domain&&a.incarnation&&a.catalog&&a.lifetime;}
static bool perms(uint8_t p){return p&&!(p&~7u)&&(p&1);}
static bool range(uint64_t base,uint64_t bytes,uint64_t limit) {
 return !(base&4095)&&bytes&&!(bytes&4095)&&base<limit&&bytes<=limit-base;
}
static bool overlap(uint64_t a,uint64_t na,uint64_t b,uint64_t nb){return a<b+nb&&b<a+na;}
static bool building(const struct nested_root_catalog *c){return c&&c->initialized&&!c->published&&!c->closed&&!c->fault;}
static bool lock(struct nested_root_catalog *c,bool cleanup) {
 if(!c||!c->initialized||(!cleanup&&__atomic_load_n(&c->fault,__ATOMIC_ACQUIRE)))return false;
 for(unsigned n=0;n<NRC_LOCK_TRIES;n++){
  uint32_t zero=0;
  if(__atomic_compare_exchange_n(&c->lock,&zero,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){
   if(!cleanup&&__atomic_load_n(&c->fault,__ATOMIC_ACQUIRE)){
    __atomic_store_n(&c->lock,0,__ATOMIC_RELEASE);return false;
   }return true;
  }
  __asm__ volatile("pause":::"memory");
 }
 __atomic_store_n(&c->fault,1,__ATOMIC_RELEASE);return false;
}
static void unlock(struct nested_root_catalog *c){__atomic_store_n(&c->lock,0,__ATOMIC_RELEASE);}
static const struct nrc_span *span_at(const struct nested_root_catalog *c,uint64_t gpa) {
 for(size_t i=0;i<c->span_count;i++){
  const struct nrc_span *s=&c->spans[i];
  if(gpa>=s->gpa&&gpa-s->gpa<s->bytes&&s->bytes-(gpa-s->gpa)>=4096)return s;
 }return NULL;
}
static const struct nrc_root *root_at(const struct nested_root_catalog *c,uint64_t key) {
 for(size_t i=0;i<c->root_count;i++)if(c->roots[i].full_eptp==key)return &c->roots[i];
 return NULL;
}
static bool init(struct nested_root_catalog *c,struct nrc_identity id,unsigned bits,
 struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,
 nrc_authority_fn authority,nrc_atomic_authority_fn atomic_authority) {
 if(!c||!id_valid(id)||bits<12||bits>48||!spans||!ns||!roots||!nr||(!authority&&!atomic_authority)||
    ns>SIZE_MAX/sizeof(*spans)||nr>SIZE_MAX/sizeof(*roots))return false;
 *c=(struct nested_root_catalog){.identity=id,.spans=spans,.span_capacity=ns,
  .roots=roots,.root_capacity=nr,.maxphys=bits,.authority=authority,.initialized=true,
  .atomic_wb=atomic_authority!=NULL,.atomic_authority=atomic_authority};return true;
}
bool nrc_init(struct nested_root_catalog *c,struct nrc_identity id,unsigned bits,
 struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,nrc_authority_fn authority) {
 return init(c,id,bits,spans,ns,roots,nr,authority,NULL);
}
bool nrc_init_atomic_wb(struct nested_root_catalog *c,struct nrc_identity id,unsigned bits,
 struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,nrc_atomic_authority_fn authority) {
 return init(c,id,bits,spans,ns,roots,nr,NULL,authority);
}
/* Metadata must not alias the predecessor or its immutable record arrays. */
static bool storage_overlap(const void *a,size_t na,const void *b,size_t nb) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return na>UINTPTR_MAX-x||nb>UINTPTR_MAX-y||(x<y+nb&&y<x+na);
}
bool nrc_init_atomic_wb_successor(struct nested_root_catalog *n,
 struct nested_root_catalog *old,struct nrc_identity old_id,struct nrc_identity next,
 unsigned bits,struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,
 nrc_atomic_authority_fn authority,uint64_t reserve) {
 if(!n||!old||n==old||n->initialized||!reserve||!id_valid(next)||
    next.domain!=old_id.domain||next.incarnation!=old_id.incarnation||
    next.catalog==old_id.catalog||next.lifetime==old_id.lifetime||
    bits<12||bits>48||!spans||!ns||!roots||!nr||!authority||
    ns>SIZE_MAX/sizeof(*spans)||nr>SIZE_MAX/sizeof(*roots)||!lock(old,false))return false;
 bool ok=identity(old->identity,old_id)&&old->published&&old->closed&&!old->superseded&&
  old->atomic_wb&&!old->atomic_uc&&!old->atomic_2m&&!old->atomic_1g&&
  !old->references&&!old->sessions&&bits==old->maxphys&&
  ns>=old->span_count&&nr>=old->root_count&&reserve<=UINT64_MAX-old->next_nonce;
 const void *fresh[]={n,spans,roots};size_t sizes[]={sizeof(*n),ns*sizeof(*spans),nr*sizeof(*roots)};
 const void *prior[]={old,old->spans,old->roots};
 size_t prior_sizes[]={sizeof(*old),old->span_capacity*sizeof(*spans),old->root_capacity*sizeof(*roots)};
 for(unsigned i=0;i<3&&ok;i++) {
  for(unsigned j=0;j<3;j++)if(storage_overlap(fresh[i],sizes[i],prior[j],prior_sizes[j]))ok=false;
  for(unsigned j=i+1;j<3;j++)if(storage_overlap(fresh[i],sizes[i],fresh[j],sizes[j]))ok=false;
 }
 if(ok){
  ok=init(n,next,bits,spans,ns,roots,nr,NULL,authority);
  if(ok){n->predecessor=old;n->next_nonce=old->next_nonce;old->superseded=true;}
 }
 unlock(old);return ok;
}
bool nrc_init_atomic_wb_2m(struct nested_root_catalog *c,struct nrc_identity id,unsigned bits,
 struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,nrc_atomic_authority_fn authority) {
 if(bits<21||!nrc_init_atomic_wb(c,id,bits,spans,ns,roots,nr,authority))return false;
 c->atomic_2m=true;return true;
}
bool nrc_init_atomic_wb_1g(struct nested_root_catalog *c,struct nrc_identity id,unsigned bits,
 struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,nrc_atomic_authority_fn authority) {
 if(bits<30||!nrc_init_atomic_wb_2m(c,id,bits,spans,ns,roots,nr,authority))return false;
 c->atomic_1g=true;return true;
}
bool nrc_init_atomic_uc(struct nested_root_catalog *c,struct nrc_identity id,unsigned bits,
 struct nrc_span *spans,size_t ns,struct nrc_root *roots,size_t nr,nrc_atomic_authority_fn authority,
 uint64_t gpa,uint64_t hpa,uint64_t epoch) {
 if(!epoch||bits<12||bits>48||!gpa||!hpa||!range(gpa,4096,UINT64_C(1)<<bits)||
    !range(hpa,4096,UINT64_C(1)<<bits)||!nrc_init_atomic_wb(c,id,bits,spans,ns,roots,nr,authority))return false;
 c->atomic_uc=true;c->uc_gpa=gpa;c->uc_hpa=hpa;c->type_epoch=epoch;return true;
}
static bool add_span(struct nested_root_catalog *c,struct nrc_span s) {
 if(!building(c)||c->span_count>=c->span_capacity||!identity(s.identity,c->identity)||
    !s.object||!s.lifetime||!perms(s.permissions)||!s.ignore_pat||!s.allowed_types||
    (s.allowed_types&~(NRC_TYPE_WB|NRC_TYPE_UC))||
    (c->atomic_wb&&s.allowed_types!=NRC_TYPE_WB&&!(c->atomic_uc&&s.allowed_types==NRC_TYPE_UC)))return false;
 if(c->atomic_uc) {
  bool target=s.gpa==c->uc_gpa&&s.hpa==c->uc_hpa;
  if(target?(s.role!=NRC_DATA||s.bytes!=4096||(s.permissions&4)||s.allowed_types!=NRC_TYPE_UC):
      (s.allowed_types!=NRC_TYPE_WB||overlap(s.gpa,s.bytes,c->uc_gpa,4096)||overlap(s.hpa,s.bytes,c->uc_hpa,4096)))return false;
 }
 uint64_t limit=UINT64_C(1)<<c->maxphys;
 if(!range(s.gpa,s.bytes,limit)||!range(s.hpa,s.bytes,limit))return false;
 if(s.role==NRC_TABLE){
  if(s.permissions!=1||s.allowed_types!=NRC_TYPE_WB||!s.table_view||
     ((uintptr_t)s.table_view&4095)||s.bytes>UINTPTR_MAX-(uintptr_t)s.table_view)return false;
 }else if(s.role!=NRC_DATA||s.table_view)return false;
 for(size_t i=0;i<c->span_count;i++){
  struct nrc_span *p=&c->spans[i];
  if(p->object==s.object||overlap(p->gpa,p->bytes,s.gpa,s.bytes)||
     overlap(p->hpa,p->bytes,s.hpa,s.bytes))return false;
 }
 c->spans[c->span_count++]=s;return true;
}
bool nrc_add_span(struct nested_root_catalog *c,struct nrc_span s) {
 return c&&!c->atomic_wb&&add_span(c,s);
}
bool nrc_add_atomic_span(struct nested_root_catalog *c,struct nrc_span s,uint64_t *writable) {
 if(!c||!c->atomic_wb||(s.role==NRC_TABLE?(!writable||writable!=s.table_view):writable!=NULL))return false;
 return add_span(c,s);
}
bool nrc_add_root(struct nested_root_catalog *c,struct nrc_root r) {
 if(!building(c)||c->root_count>=c->root_capacity||!identity(r.identity,c->identity)||
    !r.source||!r.generation||!r.lifetime)return false;
 uint64_t mask=((UINT64_C(1)<<c->maxphys)-1)&~UINT64_C(4095);
 if((r.full_eptp&~mask)!=(c->atomic_wb?0x5e:0x1e)||r.root_gpa!=(r.full_eptp&mask))return false;
 const struct nrc_span *s=span_at(c,r.root_gpa);
 if(!s||s->role!=NRC_TABLE)return false;
 for(size_t i=0;i<c->root_count;i++)
  if(c->roots[i].source==r.source||c->roots[i].full_eptp==r.full_eptp)return false;
 c->roots[c->root_count++]=r;return true;
}
bool nrc_publish(struct nested_root_catalog *c) {
 if(!building(c)||!c->span_count||!c->root_count)return false;
 if(c->atomic_uc){const struct nrc_span *s=span_at(c,c->uc_gpa);if(!s||s->allowed_types!=NRC_TYPE_UC||s->hpa!=c->uc_hpa)return false;}
 c->published=true;return true;
}
bool nrc_find(struct nested_root_catalog *c,struct nrc_identity id,uint64_t key,struct nrc_root *out) {
 if(!out||!lock(c,false))return false;
 const struct nrc_root *r=root_at(c,key);
 bool ok=c->published&&!c->closed&&identity(c->identity,id)&&r;
 if(ok)*out=*r;
 unlock(c);return ok;
}
static bool valid_locked(struct nrc_session *s,uint64_t nonce) {
 struct nested_root_catalog *c=s->catalog;
 const struct nrc_root *r=root_at(c,s->root.full_eptp);
 if(!c->published||!s->live||!nonce||s->nonce!=nonce||!r||!root_equal(*r,s->root)||
    !identity(s->root.identity,c->identity)||!c->references)return false;
 for(struct nrc_session *p=c->sessions;p;p=p->next)if(p==s)return true;
 return false;
}
bool nrc_begin(struct nested_root_catalog *c,struct nrc_root root,void *context,
 struct nrc_session *s,uint64_t *nonce) {
 if(!s||!nonce||!lock(c,false))return false;
 bool ok=false;const struct nrc_root *r=root_at(c,root.full_eptp);
 if(!c->published||c->closed||!r||!root_equal(*r,root)||s->live||
    c->references==UINT64_MAX||c->next_nonce==UINT64_MAX)goto done;
 for(struct nrc_session *p=c->sessions;p;p=p->next)if(p==s)goto done;
 *s=(struct nrc_session){.catalog=c,.next=c->sessions,.root=root,.authority_context=context,
  .nonce=c->next_nonce+1,.live=true};
 c->sessions=s;c->references++;c->next_nonce=s->nonce;*nonce=s->nonce;ok=true;
done:unlock(c);return ok;
}
bool nrc_guard(struct nrc_session *s,uint64_t nonce) {
 if(!s||!s->catalog||!lock(s->catalog,false))return false;
 struct nested_root_catalog *c=s->catalog;bool ok=valid_locked(s,nonce);unlock(c);return ok;
}
bool nrc_end(struct nrc_session *s,uint64_t nonce) {
 if(!s||!s->catalog||!lock(s->catalog,true))return false;
 struct nested_root_catalog *c=s->catalog;bool ok=valid_locked(s,nonce);
 if(ok){
  struct nrc_session **p=&c->sessions;while(*p!=s)p=&(*p)->next;
  *p=s->next;c->references--;s->live=false;s->catalog=NULL;s->next=NULL;
 }unlock(c);return ok;
}
bool nrc_close(struct nested_root_catalog *c,struct nrc_identity id) {
 if(!lock(c,false))return false;
 bool ok=c->published&&!c->closed&&identity(c->identity,id);
 if(ok)c->closed=true;
 unlock(c);return ok;
}
bool nrc_drained(struct nested_root_catalog *c,struct nrc_identity id) {
 if(!lock(c,false))return false;
 bool ok=c->published&&c->closed&&identity(c->identity,id)&&!c->references&&!c->sessions;
 unlock(c);return ok;
}
bool nrc_resolve(struct nrc_session *s,uint64_t nonce,uint64_t gpa,uint8_t need,struct nrc_page *out) {
 if(!out||!need||(need&~7u)||(gpa&4095)||!nrc_guard(s,nonce))return false;
 struct nested_root_catalog *c=s->catalog;const struct nrc_span *span=span_at(c,gpa);
 if(c->atomic_wb||!span)return false;
 struct nrc_page p={0};
 if(!c->authority(s->authority_context,span,gpa,&p)||!nrc_guard(s,nonce))return false;
 uint64_t offset=gpa-span->gpa;
 const uint64_t *view=span->role==NRC_TABLE?(const uint64_t *)((uintptr_t)span->table_view+(uintptr_t)offset):NULL;
 if(p.object!=span->object||p.lifetime!=span->lifetime||p.hpa!=span->hpa+offset||
    p.permissions!=span->permissions||p.ignore_pat!=span->ignore_pat||
    p.memory_type>7||!(span->allowed_types&(1u<<p.memory_type))||p.table_view!=view)return false;
 *out=p;return true;
}
struct walk {struct nrc_session *session;uint64_t nonce;struct nrc_page last;};
static bool resolve(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *permissions) {
 struct walk *w=context;struct nrc_page p;
 if(!nrc_resolve(w->session,w->nonce,gpa,need,&p))return false;
 w->last=p;
 *hpa=p.hpa;*view=p.table_view;*permissions=p.permissions;return true;
}
struct nested_ept_result nrc_walk(struct nrc_session *s,uint64_t nonce,uint64_t gpa,uint8_t access) {
 struct nested_ept_result r={.status=NESTED_EPT_UNSUPPORTED};
 if(!access||(access&~7u)||!nrc_guard(s,nonce)||s->catalog->atomic_wb)return r;
 struct walk w={.session=s,.nonce=nonce};struct nested_ept_policy policy={.maxphys=s->catalog->maxphys};
 /* Present supported entries always include R. Reading first validates the
  * complete backing before classifying a requested W/X permission failure. */
 r=nested_ept_walk_memory(s->root.full_eptp,gpa,1,resolve,&w,&policy);
 if(!nrc_guard(s,nonce))return (struct nested_ept_result){.status=NESTED_EPT_L0_BACKING_DENIED};
 if(r.status==NESTED_EPT_MALFORMED_UNSUPPORTED)r.status=NESTED_EPT_UNSUPPORTED;
 if(r.status==NESTED_EPT_OK){
  if((w.last.permissions&access)!=access||r.leaf_type!=w.last.memory_type||
     r.ignore_pat!=w.last.ignore_pat)r.status=NESTED_EPT_L0_BACKING_DENIED;
  else if((r.effective12&access)!=access)r.status=NESTED_EPT_L1_DENIED;
 }
 return r;
}

bool nrc_idle(struct nested_root_catalog *c,struct nrc_identity id) {
 if(!lock(c,false))return false;
 bool ok=c->published&&!c->closed&&identity(c->identity,id)&&!c->references&&!c->sessions;
 unlock(c);return ok;
}

bool nrc_resolve_atomic(struct nrc_session *s,uint64_t nonce,uint64_t gpa,uint8_t need,struct nrc_atomic_page *out) {
 if(!out||!need||(need&~7u)||(gpa&4095)||!nrc_guard(s,nonce))return false;
 struct nested_root_catalog *c=s->catalog;
 if(!c->atomic_wb||!c->atomic_authority)return false;
 const struct nrc_span *span=span_at(c,gpa);
 if(!span)return false;
 struct nrc_atomic_page p={0};
 if(!c->atomic_authority(s->authority_context,span,gpa,&p)||!nrc_guard(s,nonce))return false;
 uint64_t offset=gpa-span->gpa;
 const uint64_t *view=span->role==NRC_TABLE?(const uint64_t *)((uintptr_t)span->table_view+(uintptr_t)offset):NULL;
 if(p.backing.object!=span->object||p.backing.lifetime!=span->lifetime||
    p.backing.hpa!=span->hpa+offset||p.backing.permissions!=span->permissions||
    !p.backing.ignore_pat||p.backing.memory_type!=(c->atomic_uc&&gpa==c->uc_gpa?0:6)||
    span->allowed_types!=(1u<<p.backing.memory_type)||p.backing.table_view!=view||
    p.atomic_view!=view||(view?!p.pinned_writable:p.pinned_writable))return false;
 *out=p;return true;
}

bool nrc_successor_budget(struct nested_root_catalog *c,struct nrc_identity id,uint64_t reserve,uint64_t *nonce){
 if(!nonce||!reserve||!lock(c,false))return false;
 bool ok=c->published&&c->closed&&!c->superseded&&identity(c->identity,id)&&
  c->atomic_wb&&!c->atomic_uc&&!c->atomic_2m&&!c->atomic_1g&&!c->references&&!c->sessions&&
  reserve<=UINT64_MAX-c->next_nonce;
 if(ok)*nonce=c->next_nonce;
 unlock(c);return ok;
}
