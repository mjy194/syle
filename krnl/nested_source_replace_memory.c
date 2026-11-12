#include "nested_source_replace_memory.h"
static bool same(struct nrc_identity a,struct nrc_identity b){return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;}
static const struct nrc_span *span(struct nsrm_plan *p,uint64_t g){
 for(size_t i=0;i<p->catalog->span_count;i++){
  const struct nrc_span *s=&p->catalog->spans[i];
  if(g>=s->gpa&&g-s->gpa<s->bytes&&s->bytes-(g-s->gpa)>=4096)return s;
 }return NULL;
}
static bool authority(struct nsrm_plan *p,uint64_t g,enum nrc_role role,uint64_t **v){
 const struct nrc_span *s=span(p,g);struct nsrm_authority_page a={0};
 if(!s||s->role!=role||(role==NRC_TABLE&&s->permissions!=1)||s->allowed_types!=NRC_TYPE_WB||!s->ignore_pat||!same(s->identity,p->identity)||
    !p->authority(p->context,p->request.ticket,s,g,&a))return false;
 uint64_t *view=role==NRC_TABLE?(uint64_t *)((uintptr_t)s->table_view+(g-s->gpa)):NULL;
 if(!same(a.identity,p->identity)||a.ticket!=p->request.ticket||a.gpa!=g||a.object!=s->object||
    a.lifetime!=s->lifetime||a.hpa!=s->hpa+(g-s->gpa)||a.atomic_view!=view||
    a.permissions!=s->permissions||a.memory_type!=6||!a.ignore_pat||!a.pinned||
    (role==NRC_TABLE&&!a.guest_aliases_ro)||!a.updater_drained||
    (role==NRC_DATA&&g==p->request.new_gpa&&!a.replacement_data_safe))return false;
 if(v)*v=view;
 return true;
}
static int page(struct nsrm_plan *p,uint64_t g){for(size_t i=0;i<p->page_count;i++)if(p->pages[i].gpa==g)return (int)i;return -1;}
static uint64_t mask(struct nsrm_plan *p){return ((UINT64_C(1)<<p->catalog->maxphys)-1)&~UINT64_C(4095);}
static bool leaf(struct nsrm_plan *p,uint64_t e){
 uint64_t m=mask(p);const struct nrc_span *s=span(p,e&m);
 return (e&7)&&(e&1)&&!(e&~(m|UINT64_C(0x377)))&&
  (e&0x78)==0x70&&s&&((e&7)&~s->permissions)==0&&
  /* A retained graph can map its own source TABLE as guest-readable data.
   * Such an alias must be exactly R, never W/X, and uses the independent
   * TABLE authority. It does not become an eligible replacement target. */
  ((s->role==NRC_DATA&&authority(p,e&m,NRC_DATA,NULL))||
   (s->role==NRC_TABLE&&(e&7)==1&&authority(p,e&m,NRC_TABLE,NULL)));
}
/* Depth is fixed per page, so cycles and a DAG reused at incompatible levels
 * fail. Every registered TABLE page must belong to the retained root graph. */
static bool graph(struct nsrm_plan *p,unsigned n,unsigned d){
 struct nsrm_snapshot *s=&p->pages[n];
 if(s->reached)return s->depth==d;
 s->reached=true;s->depth=d;
 for(unsigned i=0;i<512;i++){
  uint64_t e=s->words[i];if(!(e&7))continue;
  if(!(e&1))return false;
  if(d==1){if(!leaf(p,e))return false;}
  else{
   if(e&~(mask(p)|UINT64_C(0x107)))return false;
   int child=page(p,e&mask(p));if(child<0||!graph(p,(unsigned)child,d-1))return false;
  }
 }return true;
}
static bool validate(struct nsrm_plan *p){
 struct nested_root_catalog *c=p->catalog;struct nsrm_request r=p->request;
 if(!r.ticket||r.index>=512||(r.l2_gpa&4095)||r.l2_gpa>>48||!same(c->identity,p->identity)||
    !c->atomic_wb||c->atomic_uc||c->atomic_2m||c->atomic_1g||c->maxphys<12||c->maxphys>48||
    !c->root_count||c->root_count>NSRM_MAX_ROOTS||!nrc_drained(c,p->identity))return false;
 for(size_t i=0;i<p->page_count;i++){p->pages[i].reached=false;p->pages[i].depth=0;}
 const struct nrc_root *root=NULL;
 for(size_t i=0;i<c->root_count;i++){
  const struct nrc_root *a=&c->roots[i];int n=page(p,a->root_gpa);
  if(!same(a->identity,p->identity)||(a->full_eptp&~mask(p))!=0x5e||
     (a->full_eptp&mask(p))!=a->root_gpa||n<0||!graph(p,(unsigned)n,4))return false;
  if(a->full_eptp==r.full_eptp)root=a;
 }
 for(size_t i=0;i<p->page_count;i++)if(!p->pages[i].reached)return false;
 if(!root)return false;
 int n=page(p,root->root_gpa);
 for(unsigned depth=4;depth>1;depth--){
  uint64_t e=p->pages[n].words[(r.l2_gpa>>(12+9*(depth-1)))&511];
  if(!(e&1))return false;
  n=page(p,e&mask(p));if(n<0)return false;
 }
 struct nsrm_snapshot *s=&p->pages[n];const struct nrc_span *t=span(p,r.table_gpa),*data=span(p,r.new_gpa),*old=span(p,r.expected_old&mask(p));
 if(!t||!data||!old||old->role!=NRC_DATA||data->role!=NRC_DATA||r.table_gpa!=s->gpa||r.table_hpa!=s->hpa||
    t->object!=r.table_object||t->lifetime!=r.table_lifetime||r.index!=((r.l2_gpa>>12)&511)||
    s->words[r.index]!=r.expected_old||!leaf(p,r.expected_old)||!leaf(p,r.new_leaf)||
    (r.expected_old&mask(p))==r.new_gpa||(r.new_leaf&mask(p))!=r.new_gpa||r.new_leaf&0x300||
    ((r.new_leaf&7)&~r.expected_old)||r.new_hpa!=data->hpa+(r.new_gpa-data->gpa)||
    r.new_object!=data->object||r.new_lifetime!=data->lifetime)return false;
 p->target=(unsigned)n;return true;
}
bool nsrm_prepare(struct nsrm_plan *p,struct nested_root_catalog *c,nsrm_authority_fn fn,void *ctx,struct nsrm_request r){
 if(!p||p->self||!c||!fn)return false;
 /* Volatile stores avoid a hosted memset dependency in the kernel build. */
 volatile unsigned char *bytes=(volatile unsigned char *)p;
 for(size_t i=0;i<sizeof(*p);i++)bytes[i]=0;
 p->self=p;p->catalog=c;p->identity=c->identity;p->authority=fn;p->context=ctx;p->request=r;
 if(!nrc_drained(c,c->identity)||!c->atomic_wb||c->atomic_uc||c->atomic_2m||c->atomic_1g)return false;
 for(size_t i=0;i<c->span_count;i++){
  const struct nrc_span *s=&c->spans[i];if(s->role!=NRC_TABLE)continue;
  if(s->bytes/4096>NSRM_MAX_PAGES-p->page_count)return false;
  for(uint64_t off=0;off<s->bytes;off+=4096){
   struct nsrm_snapshot *a=&p->pages[p->page_count++];a->gpa=s->gpa+off;a->hpa=s->hpa+off;
   if(!authority(p,a->gpa,NRC_TABLE,&a->view))return false;
   for(unsigned j=0;j<512;j++)a->words[j]=__atomic_load_n(&a->view[j],__ATOMIC_ACQUIRE);
  }
 }
 if(!p->page_count||!validate(p))return false;
 p->ready=true;return true;
}
bool nsrm_apply(struct nsrm_plan *p){
 if(!p||p->self!=p||!p->ready||p->started||p->applied)return false;
 p->ready=false;
 if(!validate(p))return false;
 for(size_t i=0;i<p->page_count;i++){
  struct nsrm_snapshot *s=&p->pages[i];uint64_t *v=NULL;
  if(!authority(p,s->gpa,NRC_TABLE,&v)||v!=s->view)return false;
  for(unsigned j=0;j<512;j++)if(__atomic_load_n(&v[j],__ATOMIC_ACQUIRE)!=s->words[j])return false;
 }
 struct nsrm_snapshot *target=&p->pages[p->target];uint64_t expected=p->request.expected_old;
 p->started=true;
 if(!__atomic_compare_exchange_n(&target->view[p->request.index],&expected,p->request.new_leaf,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return false;
 for(size_t i=0;i<p->page_count;i++)for(unsigned j=0;j<512;j++){
  uint64_t want=i==p->target&&j==p->request.index?p->request.new_leaf:p->pages[i].words[j];
  if(__atomic_load_n(&p->pages[i].view[j],__ATOMIC_ACQUIRE)!=want)return false;
 }
 p->applied=true;return true;
}
