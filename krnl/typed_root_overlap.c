#include "typed_root_overlap.h"
static bool identity(struct td_identity a,struct td_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool ticket(struct td_retire_ticket a,struct td_retire_ticket b){
 return identity(a.identity,b.identity)&&a.full_eptp==b.full_eptp&&a.hpa==b.hpa&&
  a.cache_generation==b.cache_generation&&a.history==b.history&&a.type_epoch==b.type_epoch&&a.nonce==b.nonce;
}
static bool completed(struct tr_local_completed c,unsigned cpu,unsigned phase){
 struct td_retire_ticket t=c.scope.ticket;
 return cpu<2&&phase<2&&c.complete&&c.scope.operation_nonce&&c.flush_serial&&
  t.identity.domain&&t.identity.incarnation&&t.identity.root_id&&t.identity.root_incarnation&&
  t.identity.allocator_lifetime&&t.identity.owner==cpu&&t.type_epoch==phase+1&&t.nonce&&
  t.cache_generation&&t.hpa&&(t.full_eptp&UINT64_C(0x000ffffffffff000))==t.hpa&&
  (t.full_eptp&4095)==0x1e&&t.history==(UINT64_C(1)<<cpu)&&c.acked==t.history&&
  c.invalidation.success&&c.invalidation.cpu==cpu&&c.invalidation.full_eptp==t.full_eptp&&
  c.invalidation.acked==t.history;
}
static bool receipt(struct tr_overlap_receipt c,unsigned cpu,unsigned phase){
 struct td_receipt r=c.receipt;struct td_retire_ticket t=c.completed.scope.ticket;
 return completed(c.completed,cpu,phase)&&r.live&&r.directory&&identity(r.identity,t.identity)&&
  r.full_eptp==t.full_eptp&&r.hpa==t.hpa&&r.cache_generation==t.cache_generation&&
  r.history==t.history&&r.acked==t.history&&r.flush_serial==c.completed.flush_serial&&
  r.type_epoch==t.type_epoch&&r.retirement_nonce==t.nonce&&c.evidence_version&&
  r.commit_version>c.evidence_version;
}
static bool hold(struct tr_overlap_hold h){
 return h.phase<2&&h.cpu==h.phase&&h.version&&h.version<UINT64_MAX&&completed(h.completed,h.cpu,h.phase);
}
static bool progress(struct tr_overlap_hold h,struct tr_overlap_peer p){
 struct tr_overlap_switch s=p.switched;
 return hold(h)&&p.phase==h.phase&&p.cpu==(h.cpu^1)&&receipt(p.retired,p.cpu,p.phase)&&
  p.retired.completed.scope.ticket.identity.domain==h.completed.scope.ticket.identity.domain&&
  p.retired.completed.scope.ticket.identity.incarnation==h.completed.scope.ticket.identity.incarnation&&
  p.retired.receipt.commit_version>h.version&&p.retired.evidence_version>=h.version&&
  p.after.creations>p.before.creations&&p.after.evictions>p.before.evictions&&p.after.misses>p.before.misses&&
  p.after.switches>p.before.switches&&p.after.switches==s.completed&&!s.stage&&
  s.pin>p.before.switch_sequence&&p.after.switch_sequence==s.sequence&&
  s.old_eptp&&s.new_eptp&&s.old_eptp!=s.new_eptp&&s.pin&&s.pin<UINT64_MAX-1&&
  s.write==s.pin+1&&s.unpin==s.write+1&&s.unpin<=s.sequence;
}
bool tr_overlap_switch_step(struct tr_overlap_switch *s,unsigned step,uint64_t next,uint64_t old){
 if(!s||step<1||step>3||s->sequence==UINT64_MAX||!next||(next&4095)!=0x1e||
    (old&&(old&4095)!=0x1e)||step!=s->stage+1)return false;
 if(step==1){s->new_eptp=next;s->old_eptp=old;s->pin=++s->sequence;s->stage=1;return true;}
 if(next!=s->new_eptp||old!=s->old_eptp)return false;
 if(step==2){s->write=++s->sequence;s->stage=2;return true;}
 if(old&&old!=next&&s->completed==UINT64_MAX)return false;
 s->unpin=++s->sequence;s->stage=0;if(old&&old!=next)s->completed++;return true;
}
bool tr_overlap_publish_hold(struct tr_overlap *o,struct tr_overlap_hold h){
 if(!o||!hold(h)||__atomic_load_n(&o->held,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&o->progress,__ATOMIC_ACQUIRE)||__atomic_load_n(&o->finished,__ATOMIC_ACQUIRE))return false;
 o->hold=h;__atomic_store_n(&o->held,1,__ATOMIC_RELEASE);return true;
}
bool tr_overlap_publish_peer(struct tr_overlap *o,struct tr_overlap_peer p){
 if(!o||!__atomic_load_n(&o->held,__ATOMIC_ACQUIRE)||__atomic_load_n(&o->progress,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&o->finished,__ATOMIC_ACQUIRE)||!progress(o->hold,p))return false;
 o->peer=p;__atomic_store_n(&o->progress,1,__ATOMIC_RELEASE);return true;
}
bool tr_overlap_finish(struct tr_overlap *o,struct tr_overlap_receipt r){
 if(!o||!__atomic_load_n(&o->held,__ATOMIC_ACQUIRE)||!__atomic_load_n(&o->progress,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&o->finished,__ATOMIC_ACQUIRE)||!progress(o->hold,o->peer)||
    !receipt(r,o->hold.cpu,o->hold.phase)||!ticket(r.completed.scope.ticket,o->hold.completed.scope.ticket)||
    r.completed.scope.operation_nonce!=o->hold.completed.scope.operation_nonce||
    r.completed.flush_serial!=o->hold.completed.flush_serial||r.evidence_version>o->hold.version||
    r.receipt.directory!=o->peer.retired.receipt.directory||
    r.receipt.commit_version<=o->peer.retired.receipt.commit_version)return false;
 o->final=r;__atomic_store_n(&o->finished,1,__ATOMIC_RELEASE);return true;
}
bool tr_overlap_valid(const struct tr_overlap *o){
 if(!o||!__atomic_load_n(&o->finished,__ATOMIC_ACQUIRE)||!__atomic_load_n(&o->held,__ATOMIC_ACQUIRE)||
    !__atomic_load_n(&o->progress,__ATOMIC_ACQUIRE))return false;
 struct tr_overlap copy={0};
 return tr_overlap_publish_hold(&copy,o->hold)&&tr_overlap_publish_peer(&copy,o->peer)&&tr_overlap_finish(&copy,o->final);
}
bool tr_overlap_receipt_equal(struct tr_overlap_receipt a,struct tr_overlap_receipt b){
 struct tr_local_completed x=a.completed,y=b.completed;
 return ticket(x.scope.ticket,y.scope.ticket)&&x.scope.operation_nonce==y.scope.operation_nonce&&
  x.complete==y.complete&&x.acked==y.acked&&x.flush_serial==y.flush_serial&&
  x.invalidation.full_eptp==y.invalidation.full_eptp&&x.invalidation.cpu==y.invalidation.cpu&&
  x.invalidation.acked==y.invalidation.acked&&x.invalidation.success==y.invalidation.success&&
  a.evidence_version==b.evidence_version&&a.receipt.directory==b.receipt.directory&&
  a.receipt.commit_version==b.receipt.commit_version&&
  receipt(a,(unsigned)x.scope.ticket.identity.owner,(unsigned)x.scope.ticket.type_epoch-1)&&
  receipt(b,(unsigned)y.scope.ticket.identity.owner,(unsigned)y.scope.ticket.type_epoch-1);
}
bool tr_overlap_wait_for(const uint32_t *word,const struct tr_overlap_wait *w){
 if(!word||!w||!w->now||!w->relax||!w->healthy||!w->lock_owned||!w->budget||
    w->lock_owned(w->context))return false;
 uint64_t start=w->now(w->context);
 for(;;){
  if(!w->healthy(w->context)||w->lock_owned(w->context))return false;
  if(__atomic_load_n(word,__ATOMIC_ACQUIRE))return true;
  if(w->now(w->context)-start>=w->budget)return false;
  w->relax(w->context);
 }
}
