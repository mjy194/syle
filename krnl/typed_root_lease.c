#include "typed_root_lease.h"

static bool identity(struct td_identity a,struct td_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool token_equal(struct trl_token a,struct trl_token b) {
 return identity(a.identity,b.identity)&&a.nonce==b.nonce&&a.type_epoch==b.type_epoch&&a.type==b.type;
}
static bool backing_equal(struct trl_backing a,struct trl_backing b) {
 return a.hpa==b.hpa&&a.allocator_lifetime==b.allocator_lifetime&&a.dedicated_data_wb==b.dedicated_data_wb;
}
static bool valid(const struct typed_root_lease *l) {
 return l&&l->directory&&!l->poisoned&&!l->directory->poisoned&&l->version&&
  l->domain==l->directory->domain&&l->incarnation==l->directory->incarnation&&
  l->type_epoch==l->directory->type_epoch&&(l->atomic_uc?(l->atomic_wb&&l->type==TRL_UC):(l->type==TRL_WB||(!l->atomic_wb&&l->type==TRL_UC)));
}
static bool operation(const struct typed_root_lease *l,const struct td_operation *o,uint64_t n) {
 if(!valid(l)||!o||!n||l->directory->sealed)return false;
 for(const struct td_operation *p=l->directory->ops;p;p=p->next)
  if(p==o)return p->live&&p->directory==l->directory&&p->nonce==n;
 return false;
}
static bool member(const struct typed_root_lease *l,const struct trl_binding *b) {
 if(!l||!b)return false;
 for(const struct trl_binding *p=l->bindings;p;p=p->next)if(p==b)return p->lease==l;
 return false;
}
static const struct trl_binding *find(const struct typed_root_lease *l,struct td_identity id) {
 for(const struct trl_binding *b=l->bindings;b;b=b->next)
  if(b->state!=TRL_RESERVED&&identity(b->token.identity,id))return b;
 return NULL;
}
static bool linked_record(const struct typed_root_lease *l,const struct td_record *r) {
 for(const struct td_record *p=l->directory->records;p;p=p->next)if(p==r)return true;
 return false;
}
static bool root(const struct typed_root_lease *l,const struct trl_binding *b) {
 if(!b->record||!linked_record(l,b->record))return false;
 const struct td_record *r=b->record;
 return r->directory==l->directory&&identity(r->identity,b->token.identity)&&
  (!b->attached||(r->attached&&r->full_eptp==b->full_eptp&&r->hpa==b->hpa));
}
static bool current(const struct typed_root_lease *l,const struct trl_binding *b) {
 return b->token_live&&b->token.nonce&&b->token.type_epoch==l->type_epoch&&b->token.type==l->type;
}
static bool change(const struct typed_root_lease *l,const struct td_operation *o,uint64_t n,const struct trl_binding *b) {
 return operation(l,o,n)&&member(l,b)&&l->version!=UINT64_MAX;
}
static bool proof(const struct typed_root_lease *l,const struct trl_binding *b) {
 return b->state==TRL_RETIRED&&!b->token_live&&b->attached&&!b->record&&
  identity(b->token.identity,b->retirement.identity)&&b->token.nonce&&
  b->retirement.nonce&&b->retirement.type_epoch==b->token.type_epoch&&
  b->retirement.type_epoch<=l->type_epoch&&b->retirement.full_eptp==b->full_eptp&&
  b->retirement.hpa==b->hpa&&b->receipt_version&&b->receipt_version<=l->directory->version&&
  td_receipt_matches(l->directory,b->receipt,b->retirement)&&b->receipt->flush_serial==b->receipt_flush_serial&&b->receipt_version==b->receipt->commit_version;
}
bool trl_init(struct typed_root_lease *l,struct typed_directory *d,struct trl_backing backing,size_t budget) {
 if(!l||!d||!budget||!d->domain||!d->incarnation||!d->type_epoch||!d->participants||
    d->closed||d->sealed||d->poisoned||d->records||d->receipts||d->operations||
    !backing.hpa||(backing.hpa&4095)||backing.hpa>=UINT64_C(0x10000000000000)||
    !backing.allocator_lifetime||!backing.dedicated_data_wb)return false;
 *l=(struct typed_root_lease){.directory=d,.backing=backing,.domain=d->domain,.incarnation=d->incarnation,
  .type_epoch=d->type_epoch,.version=1,.budget=budget,.type=TRL_WB};return true;
}
bool trl_init_atomic_wb(struct typed_root_lease *l,struct typed_directory *d,struct trl_backing b,size_t budget) {
 if(!trl_init(l,d,b,budget))return false;
 l->atomic_wb=true;return true;
}
bool trl_init_atomic_uc(struct typed_root_lease *l,struct typed_directory *d,struct trl_backing b,size_t budget) {
 if(!trl_init_atomic_wb(l,d,b,budget))return false;
 l->atomic_uc=true;l->type=TRL_UC;return true;
}
bool trl_reserve(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b,uint64_t owner,uint64_t lifetime) {
 if(!operation(l,o,n)||!b||member(l,b)||l->reserved>=l->budget||l->version==UINT64_MAX||
    l->next_nonce==UINT64_MAX||owner>=64||!(l->directory->participants&(UINT64_C(1)<<owner))||!lifetime)return false;
 *b=(struct trl_binding){.next=l->bindings,.lease=l,.owner=owner,.allocator_lifetime=lifetime,
  .constructing_nonce=n,.token={.nonce=++l->next_nonce,.type_epoch=l->type_epoch,.type=l->type},.state=TRL_RESERVED};
 l->bindings=b;l->reserved++;l->version++;return true;
}
bool trl_bind_candidate(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b,struct td_record *r,struct td_identity id) {
 if(!change(l,o,n,b)||b->state!=TRL_RESERVED||b->constructing_nonce!=n||!r||!linked_record(l,r)||
    r->directory!=l->directory||!identity(r->identity,id)||id.owner!=b->owner||
    id.allocator_lifetime!=b->allocator_lifetime||r->state!=TD_CANDIDATE||r->attached||
    r->constructing_nonce!=n||find(l,id)||l->live_tokens==SIZE_MAX)return false;
 b->record=r;b->token.identity=id;b->token_live=true;b->state=TRL_CANDIDATE;
 l->live_tokens++;l->version++;return true;
}
bool trl_attach(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b) {
 if(!change(l,o,n,b)||!current(l,b)||b->state!=TRL_CANDIDATE||b->constructing_nonce!=n||
    b->attached||!root(l,b)||!b->record->attached||b->record->hpa==l->backing.hpa||
    (l->atomic_wb&&(b->record->full_eptp&4095)!=0x5e)||
    (b->record->state!=TD_CANDIDATE&&b->record->state!=TD_RETAINED))return false;
 b->full_eptp=b->record->full_eptp;b->hpa=b->record->hpa;b->attached=true;
 b->state=b->record->state==TD_RETAINED?TRL_RETAINED:TRL_ATTACHED;l->version++;return true;
}
bool trl_publish_ready(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b) {
 return change(l,o,n,b)&&current(l,b)&&b->state==TRL_ATTACHED&&b->constructing_nonce==n&&
  b->attached&&root(l,b)&&b->record->state==TD_CANDIDATE&&b->record->constructing_nonce==n&&
  b->record->cache_generation&&!b->record->pins&&!b->record->refills;
}
bool trl_publish(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b) {
 if(!change(l,o,n,b)||!current(l,b)||b->state!=TRL_ATTACHED||b->constructing_nonce!=n||
    !b->attached||!root(l,b)||b->record->state!=TD_LIVE||!b->record->cache_generation)return false;
 b->state=TRL_LIVE;b->constructing_nonce=0;l->version++;return true;
}
bool trl_retain(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b) {
 if(!change(l,o,n,b)||!current(l,b)||!root(l,b)||b->record->state!=TD_RETAINED||
    (b->record->attached&&b->record->hpa==l->backing.hpa))return false;
 if(b->record->attached){b->attached=true;b->full_eptp=b->record->full_eptp;b->hpa=b->record->hpa;}
 b->state=TRL_RETAINED;b->constructing_nonce=0;l->version++;return true;
}
bool trl_cancel(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b) {
 if(!change(l,o,n,b)||b->attached||b->constructing_nonce!=n||
    (b->state!=TRL_RESERVED&&b->state!=TRL_CANDIDATE))return false;
 if(b->record&&(linked_record(l,b->record)||b->record->directory||b->record->attached||
    b->record->state!=TD_CANDIDATE||!identity(b->record->identity,b->token.identity)))return false;
 struct trl_binding **p=&l->bindings;while(*p!=b)p=&(*p)->next;
 *p=b->next;l->reserved--;if(b->token_live)l->live_tokens--;l->version++;
 *b=(struct trl_binding){0};return true;
}
bool trl_token_read(const struct typed_root_lease *l,const struct trl_binding *b,struct trl_token *out) {
 if(!valid(l)||!member(l,b)||!out||!current(l,b))return false;
 *out=b->token;return true;
}
bool trl_admit(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b,struct trl_token t,unsigned type,bool ipat,bool ad) {
 return operation(l,o,n)&&!l->directory->closed&&member(l,b)&&current(l,b)&&
  token_equal(t,b->token)&&type==l->type&&ipat&&ad==l->atomic_wb&&
  (!l->atomic_wb||(type==(l->atomic_uc?TRL_UC:TRL_WB)&&(b->full_eptp&4095)==0x5e))&&b->state==TRL_LIVE&&b->attached&&
  root(l,b)&&b->record->state==TD_LIVE&&b->record->cache_generation;
}
bool trl_retire(struct typed_root_lease *l,struct td_operation *o,uint64_t n,struct trl_binding *b,struct trl_token t,struct td_retire_ticket rt,const struct td_receipt *receipt,uint64_t receipt_version) {
 if(!change(l,o,n,b)||!current(l,b)||!token_equal(t,b->token)||!b->attached||!b->record||!l->live_tokens||
    !receipt||!receipt_version||receipt_version>l->directory->version)return false;
 const struct td_record *r=b->record;
 bool linked=linked_record(l,r);
 if((linked?r->directory!=l->directory:r->directory!=NULL)||!identity(r->identity,t.identity)||
    r->state!=TD_RETIRED||!r->attached||r->pins||r->refills||r->full_eptp!=b->full_eptp||r->hpa!=b->hpa||
    !identity(rt.identity,t.identity)||rt.nonce!=r->retirement_nonce||!rt.nonce||
    rt.type_epoch!=l->type_epoch||rt.type_epoch!=r->retirement_epoch||rt.full_eptp!=r->full_eptp||rt.hpa!=r->hpa||
    rt.cache_generation!=r->cache_generation||rt.history!=r->history||!td_receipt_matches(l->directory,receipt,rt)||receipt_version!=receipt->commit_version)return false;
 /* No failure below this point. Capture durable proof before releasing the
  * root token; the independent record/tree may now be forgotten/freed. */
 b->retirement=rt;b->receipt=receipt;b->receipt_version=receipt_version;b->receipt_flush_serial=receipt->flush_serial;
 b->record=NULL;b->token_live=false;b->prepared=false;b->state=TRL_RETIRED;
 l->live_tokens--;l->version++;return true;
}
static bool inventory(const struct typed_root_lease *l) {
 if(!valid(l))return false;
 size_t tokens=0,slots=0;
 for(const struct trl_binding *b=l->bindings;b;b=b->next){
  if(b->lease!=l||slots==SIZE_MAX)return false;
  slots++;
  if(b->state==TRL_RETIRED){if(!proof(l,b))return false;continue;}
  if(!current(l,b)||!root(l,b)||!b->attached||b->record->state==TD_RETIRING||
     b->record->state==TD_RETIRED||b->record->state==TD_CANDIDATE||tokens==SIZE_MAX)return false;
  if((b->state==TRL_LIVE)!=(b->record->state==TD_LIVE)||
     (b->state!=TRL_LIVE&&b->state!=TRL_RETAINED))return false;
  tokens++;
 }
 if(tokens!=l->live_tokens||slots!=l->reserved||slots>l->budget)return false;
 for(const struct td_record *r=l->directory->records;r;r=r->next){
  const struct trl_binding *b=find(l,r->identity);
  if(!b||!r->attached)return false;
  if(r->state==TD_RETIRED){
   if(!proof(l,b)||r->pins||r->refills||r->full_eptp!=b->retirement.full_eptp||r->hpa!=b->retirement.hpa||
      r->cache_generation!=b->retirement.cache_generation||r->history!=b->retirement.history||
      r->retirement_nonce!=b->retirement.nonce||r->retirement_epoch!=b->retirement.type_epoch)return false;
  }else if(b->record!=r||!b->token_live)return false;
 }
 for(const struct td_receipt *r=l->directory->receipts;r;r=r->next){
  const struct trl_binding *b=find(l,r->identity);
  if(!b||b->receipt!=r||!proof(l,b))return false;
 }
 return true;
}
bool trl_close(struct typed_root_lease *l) {
 /* Closing with an in-flight allocation could strand a later gen0 failure:
  * td intentionally forbids retirement after close. Require drain first in
  * this opt-in model rather than inventing a rollback/thaw protocol. */
 if(!valid(l)||l->directory->operations||!inventory(l))return false;
 for(const struct trl_binding *b=l->bindings;b;b=b->next)
  if(b->token_live&&(!b->record->cache_generation||b->record->refills))return false;
 return td_close(l->directory);
}
bool trl_seal(struct typed_root_lease *l,struct trl_freeze *out) {
 if(!out||!inventory(l)||l->directory->operations)return false;
 struct td_ticket t;if(!td_seal(l->directory,&t))return false;
 *out=(struct trl_freeze){.directory=t,.backing=l->backing,.lease_version=l->version,.type=l->type};return true;
}
bool trl_freeze_valid(const struct typed_root_lease *l,struct trl_freeze t) {
 return valid(l)&&backing_equal(t.backing,l->backing)&&t.lease_version==l->version&&t.type==l->type&&td_ticket_valid(l->directory,t.directory);
}
static bool freeze_equal(struct trl_freeze a,struct trl_freeze b) {
 return backing_equal(a.backing,b.backing)&&a.lease_version==b.lease_version&&a.type==b.type&&a.directory.domain==b.directory.domain&&
  a.directory.incarnation==b.directory.incarnation&&a.directory.nonce==b.directory.nonce&&
  a.directory.version==b.directory.version&&a.directory.type_epoch==b.directory.type_epoch;
}
bool trl_snapshot_read(const struct typed_root_lease *l,struct trl_freeze t,struct trl_snapshot *out,size_t cap,size_t *count) {
 if(!trl_freeze_valid(l,t)||!inventory(l)||!count)return false;
 size_t needed=0;
 for(const struct td_record *r=l->directory->records;r;r=r->next){if(needed==SIZE_MAX)return false;needed++;}
 for(const struct td_receipt *r=l->directory->receipts;r;r=r->next){if(needed==SIZE_MAX)return false;needed++;}
 if(cap<needed||(!out&&needed))return false;
 size_t i=0;
 for(const struct td_record *r=l->directory->records;r;r=r->next){
  const struct trl_binding *b=find(l,r->identity);
  out[i++]=(struct trl_snapshot){.root={.identity=r->identity,.full_eptp=r->full_eptp,.hpa=r->hpa,
   .cache_generation=r->cache_generation,.history=r->history,.pins=r->pins,.refills=r->refills,
   .state=r->state,.attached=r->attached},.token=b->token,.scan_allowed=r->state!=TD_RETIRED,
   .receipt_type_epoch=b->state==TRL_RETIRED?b->retirement.type_epoch:0,.receipt_version=b->receipt_version};
 }
 for(const struct td_receipt *r=l->directory->receipts;r;r=r->next){
  const struct trl_binding *b=find(l,r->identity);
  out[i++]=(struct trl_snapshot){.root={.identity=r->identity,.full_eptp=r->full_eptp,.hpa=r->hpa,
   .cache_generation=r->cache_generation,.history=r->history,.state=TD_RETIRED,.attached=true,.receipt=true},
   .token=b->token,.receipt_type_epoch=b->retirement.type_epoch,.receipt_version=b->receipt_version};
 }
 *count=needed;return true;
}
bool trl_snapshot_index(const struct typed_root_lease *l,struct trl_freeze t,struct td_identity id,bool receipt,size_t *out) {
 if(!trl_freeze_valid(l,t)||!inventory(l)||!out)return false;
 size_t i=0;
 for(const struct td_record *r=l->directory->records;r;r=r->next,i++)
  if(!receipt&&identity(r->identity,id)){*out=i;return true;}
 for(const struct td_receipt *r=l->directory->receipts;r;r=r->next,i++)
  if(receipt&&identity(r->identity,id)){*out=i;return true;}
 return false;
}
static bool destination(const struct typed_root_lease *l,uint64_t epoch,unsigned type) {
 return !l->atomic_wb&&epoch>l->type_epoch&&((l->type==TRL_WB&&type==TRL_UC)||(l->type==TRL_UC&&type==TRL_WB));
}
bool trl_prepare_token(struct typed_root_lease *l,struct trl_freeze f,struct trl_binding *b,struct trl_token t,uint64_t epoch,unsigned type,struct trl_token *out) {
 if(!trl_freeze_valid(l,f)||!inventory(l)||!member(l,b)||!out||!current(l,b)||
    !token_equal(t,b->token)||b->prepared||!destination(l,epoch,type)||!root(l,b)||
    !b->record->cache_generation||b->record->refills)return false;
 b->prepared=true;b->prepared_epoch=epoch;b->prepared_type=type;
 *out=b->token;out->type_epoch=epoch;out->type=type;return true;
}
bool trl_reopen(struct typed_root_lease *l,struct trl_freeze f,const struct trl_transition_evidence *e) {
 if(!e||!trl_freeze_valid(l,f)||!inventory(l)||!freeze_equal(f,e->freeze)||
    !destination(l,e->new_epoch,e->destination)||!e->serial||l->version==UINT64_MAX||
    !e->complete_alias_audit||!e->all_refills_revoked||!e->all_roots_invalidated||
    !e->host_break_before_make||!e->all_aliases_retyped||!e->owner_stamps_ready)return false;
 uint64_t all=l->directory->participants;
 if(e->closed!=all||e->drained!=all||e->tlb_before!=all||e->cache_before!=all||e->tlb_after!=all||e->cache_after!=all)return false;
 for(const struct trl_binding *b=l->bindings;b;b=b->next)
  if(b->token_live&&(!b->prepared||b->prepared_epoch!=e->new_epoch||b->prepared_type!=e->destination))return false;
 if(!td_reopen(l->directory,f.directory,e->new_epoch))return false;
 l->type_epoch=e->new_epoch;l->type=e->destination;l->version++;
 for(struct trl_binding *b=l->bindings;b;b=b->next)if(b->token_live){
  b->token.type_epoch=l->type_epoch;b->token.type=l->type;b->prepared=false;b->prepared_epoch=0;
 }
 return true;
}
bool trl_backing_release_ready(const struct typed_root_lease *l,struct trl_freeze f,const struct trl_final_evidence *e) {
 if(!l||l->atomic_wb||!e||!trl_freeze_valid(l,f)||!inventory(l)||!freeze_equal(f,e->freeze)||l->type!=TRL_WB||
    l->live_tokens||l->directory->records||!e->serial||!e->restored_wb||!e->host_wb_verified||!e->complete_alias_audit)return false;
 uint64_t all=l->directory->participants;
 return e->parked==all&&e->vmcs_gone==all&&e->refills_revoked==all&&e->roots_flushed==all&&e->caches_flushed==all;
}
void trl_poison(struct typed_root_lease *l) {if(l){l->poisoned=true;td_poison(l->directory);}}
