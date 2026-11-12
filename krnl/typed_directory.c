#include "typed_directory.h"
#define ROOT_MASK UINT64_C(0x000ffffffffff000)
static bool eq(struct td_identity a,struct td_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
 a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool operation(struct typed_directory *d,struct td_operation *o,uint64_t n) {
 if(!d||!o||!n||d->poisoned||d->sealed)return false;
 for(struct td_operation *p=d->ops;p;p=p->next)
  if(p==o)return p->live&&p->directory==d&&p->nonce==n;
 return false;
}
static bool record(struct typed_directory *d,struct td_record *r,struct td_identity id) {
 if(!d||!r)return false;
 for(struct td_record *p=d->records;p;p=p->next)
  if(p==r)return p->directory==d&&eq(p->identity,id);
 return false;
}
static bool change(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 return operation(d,o,n)&&record(d,r,id)&&d->version!=UINT64_MAX;
}
static void unlink_record(struct typed_directory *d,struct td_record *r) {
 struct td_record **p=&d->records;while(*p!=r)p=&(*p)->next;
 *p=r->next;r->next=NULL;r->directory=NULL;
}
bool td_init(struct typed_directory *d,uint64_t domain,uint64_t incarnation,uint64_t participants,uint64_t epoch) {
 if(!d||!domain||!incarnation||!participants||!epoch)return false;
 *d=(struct typed_directory){.domain=domain,.incarnation=incarnation,.participants=participants,.version=1,.type_epoch=epoch};return true;
}
bool td_begin(struct typed_directory *d,struct td_operation *o,uint64_t *n) {
 if(!d||!o||!n||!d->domain||d->closed||d->sealed||d->poisoned||d->next_nonce==UINT64_MAX||d->operations==UINT64_MAX)return false;
 for(struct td_operation *p=d->ops;p;p=p->next)if(p==o)return false;
 *o=(struct td_operation){.next=d->ops,.directory=d,.nonce=++d->next_nonce,.live=true};
 d->ops=o;d->operations++;*n=o->nonce;return true;
}
bool td_end(struct typed_directory *d,struct td_operation *o,uint64_t n) {
 if(!d||!o||!n||!d->operations)return false;
 struct td_operation **p=&d->ops;while(*p&&*p!=o)p=&(*p)->next;
 if(!*p||!o->live||o->directory!=d||o->nonce!=n)return false;
 for(struct td_record *r=d->records;r;r=r->next)if(r->state==TD_CANDIDATE&&r->constructing_nonce==n)return false;
 *p=o->next;o->next=NULL;o->directory=NULL;o->live=false;d->operations--;return true;
}
bool td_reserve(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,uint64_t owner,uint64_t lifetime,struct td_identity *id) {
 if(!operation(d,o,n)||!r||!id||owner>=64||!(d->participants&(UINT64_C(1)<<owner))||!lifetime||d->next_id==UINT64_MAX||d->version==UINT64_MAX)return false;
 for(struct td_record *p=d->records;p;p=p->next)if(p==r)return false;
 uint64_t serial=d->next_id+1;
 struct td_identity identity={d->domain,d->incarnation,serial,serial,lifetime,owner};
 *r=(struct td_record){.next=d->records,.directory=d,.identity=identity,.state=TD_CANDIDATE,.constructing_nonce=n};
 d->records=r;d->next_id=serial;d->version++;*id=identity;return true;
}
bool td_attach(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id,uint64_t eptp,uint64_t hpa,uint64_t gen,uint64_t history) {
 if(!change(d,o,n,r,id)||r->state!=TD_CANDIDATE||r->constructing_nonce!=n||r->attached||history||!hpa||(hpa&4095)||(hpa&~ROOT_MASK)||(eptp&ROOT_MASK)!=hpa||(eptp>>52))return false;
 for(struct td_record *p=d->records;p;p=p->next)if(p!=r&&p->attached&&p->state!=TD_RETIRED&&p->hpa==hpa)return false;
 r->full_eptp=eptp;r->hpa=hpa;r->cache_generation=gen;r->history=0;r->attached=true;d->version++;return true;
}
bool td_publish(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||r->state!=TD_CANDIDATE||r->constructing_nonce!=n||!r->attached||!r->cache_generation)return false;
 r->state=TD_LIVE;r->constructing_nonce=0;d->version++;return true;
}
bool td_retain(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||r->state==TD_RETIRED)return false;
 if(r->state==TD_CANDIDATE&&r->constructing_nonce!=n)return false;
 r->state=TD_RETAINED;r->constructing_nonce=0;d->version++;return true;
}
bool td_cancel(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||r->state!=TD_CANDIDATE||r->constructing_nonce!=n||r->attached)return false;
 unlink_record(d,r);d->version++;return true;
}
bool td_pin(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||r->state!=TD_LIVE||r->pins==UINT64_MAX)return false;
 r->pins++;d->version++;return true;
}
bool td_unpin(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||!r->pins)return false;
 r->pins--;d->version++;return true;
}
bool td_refill_add(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||r->state!=TD_LIVE||r->refills==UINT64_MAX)return false;
 r->refills++;d->version++;return true;
}
bool td_refill_drop(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||!r->refills)return false;
 r->refills--;d->version++;return true;
}
bool td_resident(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id,unsigned cpu) {
 if(!change(d,o,n,r,id)||r->state!=TD_LIVE||cpu>=64||!(d->participants&(UINT64_C(1)<<cpu)))return false;
 r->history|=UINT64_C(1)<<cpu;d->version++;return true;
}
bool td_cache_reset(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id,uint64_t gen) {
 if(!change(d,o,n,r,id)||r->state!=TD_LIVE||gen<=r->cache_generation)return false;
 r->cache_generation=gen;d->version++;return true;
}
bool td_retire_begin(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id,struct td_retire_ticket *ticket) {
 if(!change(d,o,n,r,id)||d->closed||!ticket||d->next_nonce==UINT64_MAX||(r->state!=TD_LIVE&&r->state!=TD_RETAINED)||r->pins||r->refills||!r->attached)return false;
 r->retirement_nonce=++d->next_nonce;r->retirement_epoch=d->type_epoch;
 *ticket=(struct td_retire_ticket){id,r->full_eptp,r->hpa,r->cache_generation,r->history,d->type_epoch,r->retirement_nonce};
 r->state=TD_RETIRING;d->version++;return true;
}
bool td_retire_pending(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id,struct td_retire_ticket t) {
 return operation(d,o,n)&&record(d,r,id)&&r->attached&&r->state==TD_RETIRING&&!r->pins&&!r->refills&&
  eq(t.identity,id)&&t.nonce&&t.nonce==r->retirement_nonce&&t.full_eptp==r->full_eptp&&
  t.hpa==r->hpa&&t.cache_generation==r->cache_generation&&t.history==r->history&&
  t.type_epoch==r->retirement_epoch&&t.type_epoch==d->type_epoch;
}
bool td_receipt_matches(const struct typed_directory *d,const struct td_receipt *r,struct td_retire_ticket t) {
 if(!d||!r||!t.nonce||!t.type_epoch||t.type_epoch>d->type_epoch)return false;
 bool found=false;for(const struct td_receipt *p=d->receipts;p;p=p->next)if(p==r){found=true;break;}
 return found&&r->live&&r->directory==d&&eq(r->identity,t.identity)&&
  r->full_eptp==t.full_eptp&&r->hpa==t.hpa&&r->cache_generation==t.cache_generation&&
  r->history==t.history&&r->acked==t.history&&r->flush_serial&&
  r->type_epoch==t.type_epoch&&r->retirement_nonce==t.nonce&&
  r->commit_version&&r->commit_version<=d->version;
}
bool td_retire_commit(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id,struct td_retire_ticket ticket,uint64_t acked,uint64_t serial,struct td_receipt *receipt) {
 if(!td_retire_pending(d,o,n,r,id,ticket)||d->version==UINT64_MAX||!receipt||!serial||acked!=ticket.history)return false;
 for(struct td_receipt *p=d->receipts;p;p=p->next)if(p==receipt||eq(p->identity,id))return false;
 *receipt=(struct td_receipt){.next=d->receipts,.directory=d,.identity=id,.full_eptp=r->full_eptp,.hpa=r->hpa,.cache_generation=r->cache_generation,.history=r->history,.acked=acked,.flush_serial=serial,
  .type_epoch=ticket.type_epoch,.retirement_nonce=ticket.nonce,.commit_version=d->version+1,.live=true};
 d->receipts=receipt;r->state=TD_RETIRED;d->version++;return true;
}
bool td_forget(struct typed_directory *d,struct td_operation *o,uint64_t n,struct td_record *r,struct td_identity id) {
 if(!change(d,o,n,r,id)||r->state!=TD_RETIRED||!r->attached||r->pins||r->refills)return false;
 struct td_retire_ticket t={id,r->full_eptp,r->hpa,r->cache_generation,r->history,r->retirement_epoch,r->retirement_nonce};
 bool found=false;for(struct td_receipt *p=d->receipts;p;p=p->next)if(td_receipt_matches(d,p,t)){found=true;break;}
 if(!found)return false;
 unlink_record(d,r);d->version++;return true;
}
bool td_close(struct typed_directory *d) {
 if(!d||!d->domain||d->closed||d->sealed||d->poisoned||d->next_nonce==UINT64_MAX)return false;
 for(struct td_record *r=d->records;r;r=r->next)if(r->state==TD_RETIRING)return false;
 d->freeze_nonce=++d->next_nonce;d->closed=true;return true;
}
bool td_seal(struct typed_directory *d,struct td_ticket *t) {
 if(!d||!t||!d->closed||d->sealed||d->poisoned||d->operations)return false;
 for(struct td_record *r=d->records;r;r=r->next)if(!r->attached||r->state==TD_RETIRING)return false;
 d->sealed=true;*t=(struct td_ticket){d->domain,d->incarnation,d->freeze_nonce,d->version,d->type_epoch};return true;
}
bool td_ticket_valid(const struct typed_directory *d,struct td_ticket t) {
 return d&&d->sealed&&d->closed&&!d->poisoned&&t.domain==d->domain&&t.incarnation==d->incarnation&&t.nonce==d->freeze_nonce&&t.version==d->version&&t.type_epoch==d->type_epoch;
}
bool td_snapshot_read(const struct typed_directory *d,struct td_ticket t,struct td_snapshot *out,size_t cap,size_t *count) {
 if(!td_ticket_valid(d,t)||!count)return false;
 size_t needed=0;for(const struct td_record *p=d->records;p;p=p->next){if(needed==SIZE_MAX)return false;needed++;}
 for(const struct td_receipt *p=d->receipts;p;p=p->next){if(needed==SIZE_MAX)return false;needed++;}
 if(cap<needed||(!out&&needed))return false;
 size_t i=0;for(const struct td_record *p=d->records;p;p=p->next)out[i++]=(struct td_snapshot){.identity=p->identity,.full_eptp=p->full_eptp,.hpa=p->hpa,.cache_generation=p->cache_generation,.history=p->history,.pins=p->pins,.refills=p->refills,.state=p->state,.attached=p->attached};
 for(const struct td_receipt *p=d->receipts;p;p=p->next)out[i++]=(struct td_snapshot){.identity=p->identity,.full_eptp=p->full_eptp,.hpa=p->hpa,.cache_generation=p->cache_generation,.history=p->history,.state=TD_RETIRED,.attached=p->hpa!=0,.receipt=true};
 *count=needed;return true;
}
bool td_reopen(struct typed_directory *d,struct td_ticket t,uint64_t epoch) {
 if(!td_ticket_valid(d,t)||epoch<=d->type_epoch||d->version==UINT64_MAX)return false;
 d->type_epoch=epoch;d->version++;d->closed=false;d->sealed=false;d->freeze_nonce=0;return true;
}
void td_poison(struct typed_directory *d) {if(d){d->poisoned=true;d->closed=true;}}
