#include "nested_atomic_catalog.h"
#include "nested_ept_atomic_format.h"
#include <stddef.h>
static bool id_equal(struct nrc_identity a,struct nrc_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;
}
static bool root_equal(struct nrc_root a,struct nrc_root b) {
 return id_equal(a.identity,b.identity)&&a.source==b.source&&a.owner==b.owner&&
  a.generation==b.generation&&a.lifetime==b.lifetime&&a.full_eptp==b.full_eptp&&a.root_gpa==b.root_gpa;
}
static bool cache_equal(struct nac_cache_identity a,struct nac_cache_identity b) {
 return a.owner==b.owner&&a.hpa==b.hpa&&a.full_eptp==b.full_eptp&&a.lifetime==b.lifetime&&a.generation==b.generation;
}
static bool stamp_equal(struct ad_entry_stamp a,struct ad_entry_stamp b) {
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.context_id==b.context_id&&
  a.epoch==b.epoch&&a.lifetime==b.lifetime;
}
static bool ticket_equal(struct nac_ticket a,struct nac_ticket b) {
 return a.source_nonce&&a.source_nonce==b.source_nonce&&a.updater_nonce==b.updater_nonce;
}
static struct nac_type_policy type_policy(const struct nested_root_catalog *c) {
 return c->atomic_uc?(struct nac_type_policy){c->type_epoch,c->uc_gpa,c->uc_hpa}:(struct nac_type_policy){0};
}
static bool policy_equal(struct nac_type_policy a,struct nac_type_policy b) {
 return a.epoch==b.epoch&&a.uc_gpa==b.uc_gpa&&a.uc_hpa==b.uc_hpa;
}
static bool policy_valid(const struct nested_root_catalog *c) {
 return c->atomic_wb&&(!c->atomic_uc||(!c->atomic_2m&&!c->atomic_1g&&c->type_epoch&&c->uc_gpa&&c->uc_hpa));
}
static void poison(struct nac_permit *p) {
 if(p->gate)ad_updater_gate_poison(p->gate);
 if(p->source.catalog)__atomic_store_n(&p->source.catalog->fault,1,__ATOMIC_RELEASE);
}
bool nac_guard(struct nac_permit *p,struct nac_ticket ticket) {
 if(!p||!p->active||!ticket_equal(ticket,p->ticket)||!ticket.updater_nonce||!p->cache_guard||
    !nrc_guard(&p->source,ticket.source_nonce)||
    !policy_valid(p->source.catalog)||!policy_equal(p->type_policy,type_policy(p->source.catalog))||
    !ad_updater_gate_guard(p->gate,&p->updater,p->stamp,ticket.updater_nonce))return false;
 if(!p->cache_guard(p->cache_context,p->cache))return false;
 return nrc_guard(&p->source,ticket.source_nonce)&&policy_valid(p->source.catalog)&&
  policy_equal(p->type_policy,type_policy(p->source.catalog))&&
  ad_updater_gate_guard(p->gate,&p->updater,p->stamp,ticket.updater_nonce);
}
bool nac_end(struct nac_permit *p,struct nac_ticket ticket) {
 if(!p||!p->active||!ticket_equal(ticket,p->ticket))return false;
 if(p->updater.live&&!ad_updater_gate_release(p->gate,&p->updater,p->stamp,ticket.updater_nonce)) {
  poison(p);return false;
 }
 if(p->source.live&&!nrc_end(&p->source,ticket.source_nonce)){poison(p);return false;}
 p->active=false;return true;
}
bool nac_complete(struct nac_permit *p,struct nac_ticket ticket) {
 bool valid=nac_guard(p,ticket);
 bool ended=nac_end(p,ticket);
 return valid&&ended;
}
bool nac_begin(struct nested_root_catalog *catalog,struct nrc_root root,void *authority_context,
 struct ad_updater_gate *gate,struct ad_entry_stamp stamp,struct nac_cache_identity cache,
 nac_cache_guard_fn guard,void *cache_context,struct nac_permit *p,struct nac_ticket *out) {
 if(!catalog||!catalog->initialized||catalog->maxphys<12||catalog->maxphys>48||!policy_valid(catalog)||!gate||!guard||!p||!out||p->active||
    p->source.live||p->updater.live||stamp.domain_id!=catalog->identity.domain||
    stamp.incarnation!=catalog->identity.incarnation||stamp.context_id!=catalog->identity.catalog||
    stamp.lifetime!=catalog->identity.lifetime||!stamp.epoch||!cache.lifetime||!cache.generation)return false;
 uint64_t mask=((UINT64_C(1)<<catalog->maxphys)-1)&~UINT64_C(4095);
 if((cache.hpa&~mask)||!cache.hpa||cache.full_eptp!=(cache.hpa|0x5e))return false;
 *p=(struct nac_permit){.type_policy=type_policy(catalog),.gate=gate,.stamp=stamp,.cache=cache,.cache_guard=guard,.cache_context=cache_context};
 if(!nrc_begin(catalog,root,authority_context,&p->source,&p->ticket.source_nonce))return false;
 p->active=true;*out=p->ticket;
 if(!ad_updater_gate_acquire(gate,stamp,&p->updater,&p->ticket.updater_nonce)) {
  /* Gate rejection alone is not poison; a failed cleanup is. */
  (void)nac_end(p,*out);return false;
 }
 *out=p->ticket;
 if(nac_guard(p,*out))return true;
 (void)nac_end(p,*out);return false;
}
struct bridge {struct nac_permit *permit;struct nac_ticket ticket;bool stale;};
static bool guard(void *context,struct ad_entry_stamp stamp) {
 struct bridge *b=context;
 bool ok=stamp_equal(stamp,b->permit->stamp)&&nac_guard(b->permit,b->ticket);
 if(!ok)b->stale=true;
 return ok;
}
static bool resolve(struct bridge *b,uint64_t gpa,uint8_t need,struct nrc_atomic_page *p) {
 if(!guard(b,b->permit->stamp))return false;
 bool ok=nrc_resolve_atomic(&b->permit->source,b->ticket.source_nonce,gpa,need,p);
 return guard(b,b->permit->stamp)&&ok;
}
static bool table(void *context,uint64_t gpa,uint64_t *hpa,uint64_t **page) {
 struct bridge *b=context;struct nrc_atomic_page p;
 if(!resolve(b,gpa,1,&p)||!p.atomic_view||!p.pinned_writable||p.backing.permissions!=1)return false;
 *hpa=p.backing.hpa;*page=p.atomic_view;return true;
}
static bool leaf(void *context,uint64_t gpa,uint64_t *hpa,uint8_t *permissions) {
 struct bridge *b=context;struct nrc_atomic_page p;
 if(!resolve(b,gpa,1,&p))return false;
 *hpa=p.backing.hpa;*permissions=p.backing.permissions;return true;
}
static bool typed_leaf(void *context,uint64_t gpa,uint64_t *hpa,uint8_t *permissions,uint8_t *type) {
 struct bridge *b=context;struct nrc_atomic_page p;
 if(!resolve(b,gpa,1,&p))return false;
 *hpa=p.backing.hpa;*permissions=p.backing.permissions;*type=p.backing.memory_type;return true;
}
struct nested_ept_atomic_result nac_walk(struct nac_permit *p,struct nac_ticket ticket,
 uint64_t gpa,uint8_t need,struct nac_trace *trace) {
 struct nested_ept_atomic_result r={.status=NEA_STALE,.translation={.status=NESTED_EPT_UNSUPPORTED}};
 if(!trace)return (struct nested_ept_atomic_result){.status=NEA_INVALID,.translation={.status=NESTED_EPT_UNSUPPORTED}};
 *trace=(struct nac_trace){0};
 if(!nac_guard(p,ticket))return r;
 struct bridge b={.permit=p,.ticket=ticket};
 const struct nested_ept_atomic_ops ops={.guard=guard,.table=table,.leaf=leaf};
 trace->type_policy=p->type_policy;trace->source=p->source.root;trace->cache=p->cache;trace->gpa=gpa;
 if(p->source.catalog->atomic_uc)r=nested_ept_atomic_walk_uc_ipat(p->source.root.full_eptp,gpa,need,
  p->source.catalog->maxphys,&ops,typed_leaf,&b,p->stamp,&trace->path);
 else r=(p->source.catalog->atomic_1g?nested_ept_atomic_walk_wb_ipat_1g:
    p->source.catalog->atomic_2m?nested_ept_atomic_walk_wb_ipat_2m:nested_ept_atomic_walk_wb_ipat)(p->source.root.full_eptp,gpa,need,p->source.catalog->maxphys,
  &ops,&b,p->stamp,&trace->path);
 if(!nac_guard(p,ticket)||b.stale)r.status=NEA_STALE;
 trace->translation=r.translation;
 trace->complete=r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK;
 return r;
}
static bool translation_equal(struct nested_ept_result a,struct nested_ept_result b) {
 return a.status==b.status&&a.l1_gpa==b.l1_gpa&&a.hpa==b.hpa&&
  a.effective12==b.effective12&&a.effective02==b.effective02&&a.leaf_resolved==b.leaf_resolved&&
  a.memory_explicit==b.memory_explicit&&a.ignore_pat==b.ignore_pat&&a.leaf_type==b.leaf_type&&a.walk_type==b.walk_type;
}
bool nac_trace_guard(struct nac_permit *p,struct nac_ticket ticket,uint64_t expected_gpa,const struct nac_trace *t) {
 /* No trace pointer is dereferenced before ALL durable identities match. */
 if(!t||!nac_guard(p,ticket)||!policy_equal(t->type_policy,p->type_policy)||!t->complete||t->gpa!=expected_gpa||!root_equal(t->source,p->source.root)||
    !cache_equal(t->cache,p->cache)||!stamp_equal(t->path.stamp,p->stamp)||(t->path.count!=4&&!(p->source.catalog->atomic_2m&&t->path.count==3)&&
     !(p->source.catalog->atomic_1g&&t->path.count==2)))return false;
 uint64_t mask=((UINT64_C(1)<<p->source.catalog->maxphys)-1)&~UINT64_C(4095);
 if(t->gpa>=(UINT64_C(1)<<p->source.catalog->maxphys))return false;
 struct bridge b={.permit=p,.ticket=ticket};uint64_t next=t->source.root_gpa;uint8_t effective=7,source_type=6;
 for(unsigned i=0;i<t->path.count;i++) {
  const struct nested_ept_atomic_entry *e=&t->path.entry[i];
  unsigned slot=(unsigned)((t->gpa>>(39-i*9))&511);
  uint64_t hpa,*page;
  if(e->depth!=i||e->entry_gpa!=next+slot*8||!table(&b,next,&hpa,&page)||
     hpa!=e->table_hpa||&page[slot]!=e->pointer||!nac_guard(p,ticket))return false;
  uint64_t now=__atomic_load_n(&page[slot],__ATOMIC_ACQUIRE);
  unsigned shift;
  if(!nac_guard(p,ticket)||!nea_decode_policy(now,i,mask,true,p->source.catalog->atomic_2m,p->source.catalog->atomic_1g,p->source.catalog->atomic_uc,&shift)||
     e->leaf!=(shift!=0)||e->leaf_shift!=shift||(shift!=0)!=(i+1==t->path.count))return false;
  uint64_t mutable=shift?0x300:0x100;
  if((now&~mutable)!=(e->raw&~mutable))return false;
  effective&=now&7;if(shift)source_type=(now>>3)&7;next=shift?nea_leaf_page(now,t->gpa,mask,shift):now&mask;
 }
 uint64_t hpa;uint8_t permissions,type;
 if(!typed_leaf(&b,next,&hpa,&permissions,&type)||type!=source_type||(type==0&&(permissions&4)))return false;
 struct nested_ept_result expected={.status=NESTED_EPT_OK,.l1_gpa=next+(t->gpa&4095),
  .hpa=hpa+(t->gpa&4095),.effective12=effective,.effective02=effective&permissions,
  .leaf_resolved=true,.memory_explicit=true,.ignore_pat=true,.leaf_type=type,.walk_type=6};
 return translation_equal(t->translation,expected)&&nac_guard(p,ticket);
}
