/* Independent N42A review. Executes the production maintenance/bridge/pools.
 * CPU, physical VMCS references and INVEPT are explicit host mocks only. */
#define main preserved_n41b_review_main
#include "typed_root_atomic_review_test.c"
#undef main
#undef RUN
#include "typed_root_atomic_maintenance.h"

struct mr_fixture {
 struct review_fixture *f;
 struct tr_atomic_maintenance m;
 struct tam_ticket ticket;
 struct nested_ept_context_item *reference[2];
 uint64_t physical_eptp[2];
 unsigned closes,opens,removes[2],restores[2],receipts[2],cas_calls,changes[9][512];
 unsigned fail_remove,fail_restore,fail_receipt,corrupt_receipt,fail_cas,compete;
 bool entry_closed,read_only_owner0,fail_final_authority,authority_failed,probe_open,only_shared;
};
static struct mr_fixture *mr_authority_fixture;
static bool mr_authority(void *p,const struct nrc_span *span,uint64_t gpa,struct nrc_atomic_page *out){
 struct mr_fixture *r=mr_authority_fixture;
 if(r&&r->authority_failed)return false;
 return authority(p,span,gpa,out);
}
static bool mr_ticket(struct mr_fixture *r,struct tam_ticket t){
 return t.nonce&&t.nonce==r->m.ticket.nonce&&
  t.stamp.domain_id==r->f->stamp.domain_id&&t.stamp.incarnation==r->f->stamp.incarnation&&
  t.stamp.context_id==r->f->stamp.context_id&&t.stamp.lifetime==r->f->stamp.lifetime&&
  t.stamp.epoch==r->f->stamp.epoch;
}
static bool mr_close(void *p,struct tam_ticket t){
 struct mr_fixture *r=p;assert(!lock_depth&&mr_ticket(r,t)&&!r->entry_closed);
 r->closes++;r->entry_closed=true;return true;
}
static bool mr_stopped(void *p,unsigned cpu,struct tam_ticket t){
 struct mr_fixture *r=p;assert(!lock_depth&&cpu<2);
 return mr_ticket(r,t)&&r->entry_closed&&!r->f->owners[cpu].running;
}
static bool mr_remove(void *p,unsigned cpu,struct tam_ticket t){
 struct mr_fixture *r=p;struct review_owner *o=&r->f->owners[cpu];
 assert(!lock_depth&&physical_cpu==cpu&&mr_stopped(p,cpu,t));r->removes[cpu]++;
 assert(r->reference[cpu]&&r->reference[cpu]==o->registry.active);
 assert(r->physical_eptp[cpu]==r->reference[cpu]->tree.eptp);
 assert(r->reference[cpu]->pins==1&&binding(o)->slot->directory.record.pins==1);
 if(r->fail_remove==cpu+1)return false;
 r->reference[cpu]=NULL;r->physical_eptp[cpu]=0;return true;
}
static bool mr_flush(void *p,unsigned cpu,struct tam_ticket t,const struct tam_root *root,struct tam_flush_receipt *out){
 struct mr_fixture *r=p;struct review_owner *o=&r->f->owners[cpu];
 assert(!lock_depth&&physical_cpu==cpu&&mr_stopped(p,cpu,t));
 assert(root&&root->item&&!root->item->pins&&!root->binding->slot->directory.record.pins);
 assert(!r->reference[cpu]&&!root->binding->slot->directory.record.refills);
 assert(root->full_eptp==root->item->tree.eptp&&root->old_generation==root->item->tree.generation);
 assert(root->new_generation==root->old_generation+1);
 assert(root->history==(UINT64_C(1)<<cpu));
 /* Exact receipt validation must precede freeing lower nodes. */
 assert(root->item->tree.nodes&&root->item->tree.leaves&&o->pool.live);
 r->receipts[cpu]++;
 *out=(struct tam_flush_receipt){.ticket=t,.identity=root->identity,.full_eptp=root->full_eptp,
  .hpa=root->hpa,.history=root->history,.old_generation=root->old_generation,
  .new_generation=root->new_generation,.acked=root->history,.cpu=cpu,.success=true};
 switch(r->corrupt_receipt){
 case 1:out->ticket.nonce++;break;case 2:out->ticket.stamp.epoch++;break;
 case 3:out->cpu^=1;break;case 4:out->full_eptp^=0x1000;break;case 5:out->full_eptp^=0x40;break;
 case 6:out->hpa^=0x1000;break;case 7:out->old_generation++;break;case 8:out->new_generation++;break;
 case 9:out->identity.allocator_lifetime++;break;case 10:out->identity.owner^=1;break;
 case 11:out->identity.root_incarnation++;break;case 12:out->acked^=3;break;
 case 13:out->history^=3;break;case 14:out->success=false;break;
 case 15:out->identity.domain++;break;case 16:out->identity.incarnation++;break;
 case 17:out->identity.root_id++;break;case 18:out->ticket.stamp.lifetime++;break;
 case 19:out->acked=3;break;
 }
 return r->fail_receipt!=cpu+1;
}
static bool mr_restore(void *p,unsigned cpu,struct tam_ticket t,const struct tam_root *root){
 struct mr_fixture *r=p;struct review_owner *o=&r->f->owners[cpu];
 assert(!lock_depth&&physical_cpu==cpu&&mr_stopped(p,cpu,t));r->restores[cpu]++;
 assert(!r->reference[cpu]&&!r->physical_eptp[cpu]&&root->item==o->registry.active);
 assert(root->item->pins==1&&binding(o)->slot->directory.record.pins==1);
 if(r->fail_restore==cpu+1)return false;
 r->reference[cpu]=root->item;r->physical_eptp[cpu]=root->full_eptp;return true;
}
static bool mr_open(void *p,struct tam_ticket t){
 struct mr_fixture *r=p;assert(!lock_depth&&mr_ticket(r,t)&&r->entry_closed);
 for(unsigned i=0;i<2;i++){
  assert(r->reference[i]&&r->physical_eptp[i]==r->reference[i]->tree.eptp);
  assert(r->f->owners[i].atomic.config.stamp.epoch==r->f->stamp.epoch+1);
 }
 assert(ad_updater_gate_open(&r->f->gate,r->m.next_stamp));
 assert(r->m.state!=TAM_OPEN);
 if(r->probe_open)for(unsigned cpu=0;cpu<2;cpu++){
  physical_cpu=cpu;struct review_owner *o=&r->f->owners[cpu];
  assert(!tr_atomic_entry(&o->atomic,r->reference[cpu],r->physical_eptp[cpu]));
  assert(!o->atomic.poisoned&&!o->registry.poisoned&&!o->bridge.directory.faulted);
 }
 physical_cpu=0;r->entry_closed=false;r->opens++;return true;
}
static uint64_t mr_load(void *p,uint64_t *word){(void)p;assert(!lock_depth);return __atomic_load_n(word,__ATOMIC_ACQUIRE);}
static bool mr_cas(void *p,uint64_t *word,uint64_t expected,uint64_t desired){
 struct mr_fixture *r=p;assert(!lock_depth&&r->entry_closed);r->cas_calls++;
 assert((expected&~UINT64_C(0x300))==(desired&~UINT64_C(0x300)));
 if(r->fail_cas&&r->cas_calls>=r->fail_cas)return false;
 if(r->compete){
  r->compete--;__atomic_fetch_xor(word,UINT64_C(0x100),__ATOMIC_ACQ_REL);return false;
 }
 bool ok=__atomic_compare_exchange_n(word,&expected,desired,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 if(ok){
  ptrdiff_t offset=word-&review_tables[0][0];assert(offset>=0&&offset<9*512);
  r->changes[offset/512][offset%512]++;
  if(r->fail_final_authority&&word==&review_tables[8][15])r->authority_failed=true;
 }
 return ok;
}
static struct mr_fixture *mr_new(void){
 struct mr_fixture *r=calloc(1,sizeof(*r));assert(r);r->f=fixture();
 mr_authority_fixture=r;r->f->catalog.atomic_authority=mr_authority;
 struct tam_hooks hooks={.close_entry=mr_close,.stopped=mr_stopped,.remove_refs=mr_remove,
  .flush=mr_flush,.restore_refs=mr_restore,.open_entry=mr_open,.load=mr_load,.cas=mr_cas};
 assert(tam_init(&r->m,&r->f->owners[0].atomic,&r->f->owners[1].atomic,&hooks,r));
 return r;
}
static void mr_init_storage(void){
 for(unsigned alias=0;alias<2;alias++){
  struct mr_fixture *r=calloc(1,sizeof(*r));assert(r);r->f=fixture();
  struct tam_hooks hooks={.close_entry=mr_close,.stopped=mr_stopped,.remove_refs=mr_remove,
   .flush=mr_flush,.restore_refs=mr_restore,.open_entry=mr_open,.load=mr_load,.cas=mr_cas};
  memset(&r->m,0xa5,sizeof(r->m));r->m.state=TAM_UNINITIALIZED;r->m.hooks=hooks;
  struct tr_atomic_maintenance *before=malloc(sizeof(*before)),*expected=calloc(1,sizeof(*expected));
  assert(before&&expected);memcpy(before,&r->m,sizeof(*before));
  struct tam_hooks bad=hooks;bad.close_entry=NULL;
  assert(!tam_init(&r->m,&r->f->owners[0].atomic,&r->f->owners[1].atomic,&bad,r));
  assert(!memcmp(before,&r->m,sizeof(*before)));
  assert(!r->f->owners[0].atomic.maintenance&&!r->f->owners[1].atomic.maintenance);
  assert(tam_init(&r->m,&r->f->owners[0].atomic,&r->f->owners[1].atomic,alias?&r->m.hooks:&hooks,r));
  expected->owners[0]=&r->f->owners[0].atomic;expected->owners[1]=&r->f->owners[1].atomic;
  expected->hooks=hooks;expected->context=r;expected->state=TAM_OPEN;expected->type_epoch=r->f->lease.type_epoch;
  expected->source_count=r->f->catalog.root_count;
  for(size_t n=0;n<expected->source_count;n++)expected->sources[n]=r->f->roots[n];
  assert(!memcmp(expected,&r->m,sizeof(*expected)));
  assert(r->f->owners[0].atomic.maintenance==&r->m&&r->f->owners[1].atomic.maintenance==&r->m);
  clean(r->f);free(expected);free(before);free(r);
 }
 puts("PASS review N42A dirty coordinator initialization preserves aliased hooks, resets all storage, and rejects atomically");
}
static void mr_populate(struct mr_fixture *r){
 for(unsigned cpu=0;cpu<2;cpu++){
  struct review_owner *o=&r->f->owners[cpu];
  for(unsigned j=0;j<2;j++){
   select_root(o,r->only_shared?(cpu+j)%2:cpu+j);(void)fault(o,1,TR_ATOMIC_FILLED);
   if(!(r->read_only_owner0&&cpu==0&&j==1))(void)fault(o,2,TR_ATOMIC_PROMOTED);
   struct nested_ept_context_item *pin=nested_ept_contexts_pin_active(&o->registry);assert(pin);
   assert(tr_atomic_entry(&o->atomic,pin,pin->tree.eptp));
   if(!j)assert(nested_ept_contexts_unpin(&o->registry,pin));
   else {r->reference[cpu]=pin;r->physical_eptp[cpu]=pin->tree.eptp;}
  }
 }
 physical_cpu=0;
 assert((review_tables[3][15]&0x300)==0x300);
 if(!r->only_shared)assert((review_tables[8][15]&0x300)==0x300);
 assert(r->f->catalog.references==4&&!r->f->gate.references);
}
static void mr_close_drain(struct mr_fixture *r){
 physical_cpu=0;assert(tam_close(&r->m,&r->ticket));
 assert(tam_drain(&r->m,r->ticket)==TAM_DRAIN_READY);
 assert(r->entry_closed&&!r->f->directory.closed&&!r->f->directory.sealed);
 assert(ad_updater_gate_drained(&r->f->gate,r->f->stamp));
 assert(!nrc_idle(&r->f->catalog,r->f->id)&&!r->f->catalog.closed);
}
static void mr_revoke(struct mr_fixture *r){
 for(unsigned cpu=0;cpu<2;cpu++){physical_cpu=cpu;assert(tam_revoke(&r->m,cpu,r->ticket));}
 physical_cpu=0;
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned j=0;j<REVIEW_TRACES;j++)
  assert(!r->f->owners[cpu].traces[j].registered&&!r->f->owners[cpu].traces[j].usable);
}
static void mr_clear(struct mr_fixture *r){physical_cpu=0;assert(tam_clear(&r->m,r->ticket));}
static void mr_reset(struct mr_fixture *r){
 for(unsigned cpu=0;cpu<2;cpu++){physical_cpu=cpu;assert(tam_reset(&r->m,cpu,r->ticket));}
 physical_cpu=0;
}
static void mr_prepare(struct mr_fixture *r){
 for(unsigned cpu=0;cpu<2;cpu++){physical_cpu=cpu;assert(tam_prepare(&r->m,cpu,r->ticket));}
 physical_cpu=0;
}
static void mr_clean(struct mr_fixture *r){
 for(unsigned cpu=0;cpu<2;cpu++){
  physical_cpu=cpu;assert(nested_ept_contexts_unpin(&r->f->owners[cpu].registry,r->reference[cpu]));
  r->reference[cpu]=NULL;
 }
 clean(r->f);free(r);
}
static void mr_cycle(void){
 struct mr_fixture *r=mr_new();mr_populate(r);
 struct nrc_root roots[3];memcpy(roots,r->f->roots,sizeof(roots));
 uint64_t words[9][512];
 for(unsigned p=0;p<9;p++)for(unsigned n=0;n<512;n++)words[p][n]=__atomic_load_n(&review_tables[p][n],__ATOMIC_ACQUIRE);
 uint64_t type=r->f->lease.type_epoch,source_refs=r->f->catalog.references;
 mr_close_drain(r);mr_revoke(r);mr_clear(r);
 assert(!(review_tables[3][15]&0x300)&&!(review_tables[8][15]&0x300));
 assert(r->m.page_count==9);
 for(unsigned p=0;p<9;p++)for(unsigned n=0;n<512;n++){
  uint64_t mask=(words[p][n]&7)?(p==3||p==8?0x300:0x100):0;
  assert(__atomic_load_n(&review_tables[p][n],__ATOMIC_ACQUIRE)==(words[p][n]&~mask));
  assert(r->changes[p][n]<=1);
 }
 assert(r->changes[3][15]==1&&r->changes[8][15]==1);
 mr_reset(r);assert(r->receipts[0]==2&&r->receipts[1]==2);mr_prepare(r);
 assert(tam_finish(&r->m,r->ticket)&&r->m.state==TAM_OPEN&&!r->entry_closed&&r->opens==1);
 assert(r->f->gate.stamp.epoch==r->f->stamp.epoch+1);
 assert(r->f->lease.type_epoch==type&&r->f->directory.type_epoch==type);
 assert(!memcmp(roots,r->f->roots,sizeof(roots))&&r->f->catalog.references==source_refs);
 for(unsigned cpu=0;cpu<2;cpu++){
  struct review_owner *o=&r->f->owners[cpu];physical_cpu=cpu;
  assert(tr_atomic_entry(&o->atomic,r->reference[cpu],r->physical_eptp[cpu]));
  assert(fault(o,1,TR_ATOMIC_FILLED).status==NESTED_EPT_OK);
  assert(!(physical_leaf(o,REVIEW_GPA)&2));
  assert((review_tables[cpu?8:3][15]&0x300)==0x100);
 }
 for(unsigned cpu=0;cpu<2;cpu++){
  struct review_owner *o=&r->f->owners[cpu];(void)fault(o,2,TR_ATOMIC_PROMOTED);
  assert((review_tables[cpu?8:3][15]&0x300)==0x300);
 }
 mr_clean(r);puts("PASS review N42A all-owner production clear/reset/retrack, shared and distinct paths, unchanged source/type identity");
}

static void mr_switch(struct mr_fixture *r,unsigned cpu,unsigned root){
 struct review_owner *o=&r->f->owners[cpu];struct nested_ept_context_item *old=r->reference[cpu];
 select_root(o,root);struct nested_ept_context_item *next=nested_ept_contexts_pin_active(&o->registry);assert(next);
 r->reference[cpu]=next;r->physical_eptp[cpu]=next->tree.eptp;
 assert(nested_ept_contexts_unpin(&o->registry,old));
 assert(tr_atomic_entry(&o->atomic,next,next->tree.eptp));
}
static void mr_all_resident_retrack(void){
 struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);
 assert(tam_finish(&r->m,r->ticket));
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned j=0;j<2;j++){
  struct review_owner *o=&r->f->owners[cpu];mr_switch(r,cpu,cpu+j);
  assert(o->registry.active->tree.generation==2);
  (void)fault(o,1,TR_ATOMIC_FILLED);assert(!(physical_leaf(o,REVIEW_GPA)&2));
  assert((review_tables[cpu+j==2?8:3][15]&0x300)==0x100);
 }
 assert((review_tables[3][15]&0x300)==0x100&&(review_tables[8][15]&0x300)==0x100);
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned j=0;j<2;j++){
  struct review_owner *o=&r->f->owners[cpu];mr_switch(r,cpu,cpu+j);
  assert(!(physical_leaf(o,REVIEW_GPA)&2));(void)fault(o,2,TR_ATOMIC_PROMOTED);
  assert(physical_leaf(o,REVIEW_GPA)&2);
 }
 assert((review_tables[3][15]&0x300)==0x300&&(review_tables[8][15]&0x300)==0x300);
 for(unsigned cpu=0;cpu<2;cpu++)for(size_t n=0;n<r->m.root_count[cpu];n++){
  struct tam_root *root=&r->m.roots[cpu][n];
  assert(root->item->tree.eptp==root->full_eptp&&root->item->tree.generation==root->new_generation);
  assert(root->binding->slot->directory.identity.allocator_lifetime==root->identity.allocator_lifetime);
 }
 mr_clean(r);puts("PASS review N42A all four retained owner-private roots independently read-retrack RO/A1D0 then first-write-promote D1");
}

static void mr_reopen_probe(void){
 struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);
 r->probe_open=true;assert(tam_finish(&r->m,r->ticket));mr_clean(r);
 puts("PASS review N42A speculative entry during physical reopen remains denied without poisoning unpublished maintenance");
}

static void mr_receipt_failures(void){
 for(unsigned mode=1;mode<=19;mode++){
  struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);mr_clear(r);
  struct review_owner *o=&r->f->owners[0];unsigned frees=o->frees;size_t live=o->pool.live;
  r->corrupt_receipt=mode;physical_cpu=0;
  assert(!tam_reset(&r->m,0,r->ticket));
  assert(o->frees==frees&&o->pool.live==live&&r->m.state==TAM_POISONED&&r->entry_closed);
  assert(!r->m.reset&&!r->opens&&r->f->gate.stamp.epoch==r->f->stamp.epoch);
  assert(!tam_finish(&r->m,r->ticket));
 }
 puts("PASS review N42A 19 exact receipt corruptions rejected before lower-page free and epoch advance");
}

static void mr_closed_admission(struct mr_fixture *r){
 for(unsigned cpu=0;cpu<2;cpu++){
  struct review_owner *o=&r->f->owners[cpu];physical_cpu=cpu;
  unsigned calls=o->authorities,allocs=o->allocations,flushes=o->flushes;
  struct nested_ept_result result;
  assert(tr_atomic_fault(&o->atomic,o->registry.active,REVIEW_GPA,2,&result)==TR_ATOMIC_REJECTED);
  assert(nested_ept_contexts_select(&o->registry,r->f->roots[0].full_eptp)!=NESTED_CONTEXT_OK);
  assert(!nested_ept_contexts_pin_active(&o->registry));
  assert(!tr_atomic_entry(&o->atomic,o->registry.active,o->registry.active->tree.eptp));
  assert(calls==o->authorities&&allocs==o->allocations&&flushes==o->flushes);
  assert(!o->atomic.poisoned&&!o->registry.poisoned&&!o->bridge.directory.faulted);
 }
 physical_cpu=0;
}
static void mr_bad_ticket(struct mr_fixture *r){
 for(unsigned n=0;n<6;n++){
  struct tam_ticket t=r->ticket;
  switch(n){case 0:t.nonce++;break;case 1:t.stamp.domain_id++;break;case 2:t.stamp.incarnation++;break;
   case 3:t.stamp.context_id++;break;case 4:t.stamp.epoch++;break;case 5:t.stamp.lifetime++;break;}
  struct tr_atomic_maintenance before=r->m;
  assert(tam_drain(&r->m,t)==TAM_DRAIN_FAILED);
  assert(!tam_revoke(&r->m,0,t)&&!tam_clear(&r->m,t)&&!tam_reset(&r->m,0,t));
  assert(!tam_prepare(&r->m,0,t)&&!tam_finish(&r->m,t));
  assert(!memcmp(&before,&r->m,sizeof(before)));
 }
}
static void mr_order_and_tickets(void){
 struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_closed_admission(r);mr_bad_ticket(r);
 struct tr_atomic_maintenance before=r->m;
 physical_cpu=1;assert(!tam_revoke(&r->m,0,r->ticket));physical_cpu=0;
 assert(!memcmp(&before,&r->m,sizeof(before)));
 assert(!tam_clear(&r->m,r->ticket)&&!tam_reset(&r->m,0,r->ticket)&&!tam_prepare(&r->m,0,r->ticket)&&!tam_finish(&r->m,r->ticket));
 assert(!memcmp(&before,&r->m,sizeof(before)));
 assert(tam_revoke(&r->m,0,r->ticket));before=r->m;
 assert(!tam_revoke(&r->m,0,r->ticket)&&!tam_clear(&r->m,r->ticket)&&!tam_finish(&r->m,r->ticket));
 assert(!memcmp(&before,&r->m,sizeof(before)));
 physical_cpu=1;assert(tam_revoke(&r->m,1,r->ticket));physical_cpu=0;
 mr_clear(r);mr_bad_ticket(r);mr_closed_admission(r);assert(tam_reset(&r->m,0,r->ticket));before=r->m;
 assert(!tam_reset(&r->m,0,r->ticket)&&!tam_finish(&r->m,r->ticket));
 assert(!memcmp(&before,&r->m,sizeof(before)));
 physical_cpu=1;assert(tam_reset(&r->m,1,r->ticket));physical_cpu=0;
 mr_prepare(r);mr_bad_ticket(r);mr_closed_admission(r);assert(tam_finish(&r->m,r->ticket));
 before=r->m;assert(!tam_finish(&r->m,r->ticket));assert(!memcmp(&before,&r->m,sizeof(before)));
 mr_clean(r);puts("PASS review N42A complete tickets, duplicate/out-of-order rejection, and normal entry/refill/context admission closed throughout");
}

static void mr_failure_retention(void){
 for(unsigned kind=0;kind<6;kind++){
  struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);
  unsigned frees[2]={r->f->owners[0].frees,r->f->owners[1].frees};
  if(kind==0){
   r->fail_remove=1;assert(!tam_revoke(&r->m,0,r->ticket));
   assert(r->reference[0]&&r->reference[0]->pins==1);
   assert(binding(&r->f->owners[0])->slot->directory.record.pins==1);
  }else if(kind==1){
   struct review_owner *o=&r->f->owners[0];struct td_operation op={0};uint64_t nonce=0;
   enter(o);assert(td_begin(&r->f->directory,&op,&nonce));
   assert(td_refill_add(&r->f->directory,&op,nonce,&binding(o)->slot->directory.record,binding(o)->slot->directory.identity));
   assert(td_end(&r->f->directory,&op,nonce));leave(o);
   assert(!tam_revoke(&r->m,0,r->ticket));
   assert(binding(o)->slot->directory.record.refills==1);
  }else if(kind==2){
   binding(&r->f->owners[0])->slot->directory.record.pins++;
   assert(!tam_revoke(&r->m,0,r->ticket));assert(r->removes[0]==0&&r->reference[0]);
  }else{
   mr_revoke(r);
   if(kind==3){
    r->fail_cas=3;assert(!tam_clear(&r->m,r->ticket));
    assert(r->m.changed==2&&r->cas_calls>=3&&r->f->gate.stamp.epoch==r->f->stamp.epoch);
   }else{
    mr_clear(r);
    if(kind==4){
     r->f->owners[0].fail_flush=r->f->owners[0].flushes+1;
     assert(!tam_reset(&r->m,0,r->ticket));assert(!r->receipts[0]);
    }else{
     mr_reset(r);r->fail_restore=1;
     frees[0]=r->f->owners[0].frees;frees[1]=r->f->owners[1].frees;
     assert(!tam_prepare(&r->m,0,r->ticket));
     assert(r->f->owners[0].registry.active->pins==1);
     assert(binding(&r->f->owners[0])->slot->directory.record.pins==1);
    }
   }
  }
  assert(r->m.state==TAM_POISONED&&r->entry_closed&&!r->opens);
  assert(r->f->gate.stamp.epoch==r->f->stamp.epoch);
  for(unsigned cpu=0;cpu<2;cpu++){
   assert(r->f->owners[cpu].frees==frees[cpu]);
   assert(r->f->owners[cpu].pool.live&&r->f->owners[cpu].atomic.poisoned);
  }
  assert(!tam_finish(&r->m,r->ticket));
 }
 puts("PASS review N42A missing references/pins/refills, partial CAS clear, sparse flush and uncertain restore retain resources and prohibit entry");
}
static void mr_source_structure(void){
 for(unsigned kind=0;kind<7;kind++){
  struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);
  switch(kind){
  case 0:__atomic_fetch_or(&review_tables[0][0],UINT64_C(0x200),__ATOMIC_ACQ_REL);break;
  case 1:__atomic_store_n(&review_tables[0][0],UINT64_C(0x1007),__ATOMIC_RELEASE);break;
  case 2:__atomic_store_n(&review_tables[4][0],UINT64_C(0x3007),__ATOMIC_RELEASE);break;
  case 3:__atomic_fetch_or(&review_tables[1][0],UINT64_C(0x80),__ATOMIC_ACQ_REL);break;
  case 4:__atomic_fetch_and(&review_tables[3][15],~UINT64_C(0x40),__ATOMIC_ACQ_REL);break;
  case 5:__atomic_fetch_xor(&review_tables[8][15],UINT64_C(0x10),__ATOMIC_ACQ_REL);break;
  case 6:__atomic_store_n(&review_tables[2][0],UINT64_C(0x90007),__ATOMIC_RELEASE);break;
  }
  assert(!tam_clear(&r->m,r->ticket));
  assert(!r->cas_calls&&r->m.state==TAM_POISONED&&r->entry_closed&&!r->opens);
 }
 struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);
 __atomic_store_n(&review_tables[2][211],UINT64_C(0x8000000000000300),__ATOMIC_RELEASE);
 r->compete=1;mr_clear(r);
 assert(review_tables[2][211]==UINT64_C(0x8000000000000300));
 assert(!(review_tables[3][15]&0x300)&&!(review_tables[8][15]&0x300));
 assert(r->cas_calls&&r->compete==0);
 mr_reset(r);mr_prepare(r);assert(tam_finish(&r->m,r->ticket));mr_clean(r);
 puts("PASS review N42A nonleaf D/cycles/depth/unsupported structure rejected before CAS, absent payload unchanged, forced linearizable CAS conflict retried");
}
static void mr_final_authority(void){
 struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);r->fail_final_authority=true;
 assert(!tam_clear(&r->m,r->ticket));
 assert(r->authority_failed&&r->m.changed&&r->m.state==TAM_POISONED&&r->entry_closed);
 assert(!tam_reset(&r->m,0,r->ticket)&&!tam_finish(&r->m,r->ticket));
 puts("PASS review N42A authority loss after final clear CAS rejects success and retains partial effects");
}

static void mr_final_source_identity(void){
 for(unsigned target=0;target<2;target++)for(unsigned field=0;field<8;field++){
  struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);
  /* Neither root is the B source pin used by the global page resolver. */
  struct nrc_root *root=&r->f->roots[target?2:0];
  switch(field){
  case 0:root->generation++;break;case 1:root->lifetime++;break;case 2:root->source++;break;
  case 3:root->owner++;break;case 4:root->full_eptp^=0x1000;break;case 5:root->root_gpa^=0x1000;break;
  case 6:root->identity.catalog++;break;case 7:root->identity.lifetime++;break;
  }
  unsigned frees[2]={r->f->owners[0].frees,r->f->owners[1].frees};
  assert(!tam_finish(&r->m,r->ticket));assert(r->entry_closed&&!r->opens&&r->m.state!=TAM_OPEN);
  assert(r->f->gate.stamp.epoch==r->f->stamp.epoch);
  for(unsigned c=0;c<2;c++)assert(r->f->owners[c].frees==frees[c]&&r->reference[c]&&r->reference[c]->pins==1);
 }
 for(unsigned kind=0;kind<2;kind++){
  struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);
  if(kind==0)__atomic_fetch_or(&review_tables[8][15],UINT64_C(0x200),__ATOMIC_ACQ_REL);
  else __atomic_store_n(&review_tables[8][211],UINT64_C(0x300),__ATOMIC_RELEASE);
  assert(!tam_finish(&r->m,r->ticket)&&r->entry_closed&&!r->opens);
  assert(r->f->gate.stamp.epoch==r->f->stamp.epoch);
 }
 for(unsigned stage=0;stage<2;stage++){
  struct mr_fixture *r=mr_new();r->only_shared=true;mr_populate(r);mr_close_drain(r);mr_revoke(r);
  if(stage){mr_clear(r);mr_reset(r);mr_prepare(r);}
  r->f->roots[2].generation++;
  if(stage)assert(!tam_finish(&r->m,r->ticket));else assert(!tam_clear(&r->m,r->ticket)&&!r->cas_calls);
  assert(r->entry_closed&&!r->opens&&r->f->gate.stamp.epoch==r->f->stamp.epoch);
 }
 puts("PASS review N42A 16 non-resolver root identity corruptions plus unselected source and late dirty/absent-word changes reject progress before epoch advance");
}

static void mr_stale_trace(void){
 for(unsigned kind=0;kind<3;kind++){
  struct mr_fixture *r=mr_new();mr_populate(r);
  struct review_owner *o=&r->f->owners[0];struct tr_atomic_trace *old=NULL;
  for(unsigned n=0;n<REVIEW_TRACES;n++)if(o->traces[n].registered&&o->traces[n].binding==binding(o))old=&o->traces[n];
  assert(old);struct nac_trace saved=old->trace;
  mr_close_drain(r);mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);assert(tam_finish(&r->m,r->ticket));
  (void)fault(o,1,TR_ATOMIC_FILLED);assert((review_tables[3][15]&0x300)==0x100);
  struct tr_atomic_trace *fresh=NULL;
  for(unsigned n=0;n<REVIEW_TRACES;n++)if(o->traces[n].registered&&o->traces[n].binding==binding(o))fresh=&o->traces[n];
  assert(fresh&&fresh!=old&&old->used&&!old->registered&&!old->usable);
  if(kind==0)fresh->trace=saved;
  if(kind==1)fresh->trace.path.stamp=saved.path.stamp;
  if(kind==2)fresh->trace.cache.generation=saved.cache.generation;
  if(kind==0)fresh->trace.path.entry[0].pointer=(void *)1;
  unsigned flushes=o->flushes,frees=o->frees;(void)fault(o,2,TR_ATOMIC_FATAL);
  assert(o->flushes==flushes&&o->frees==frees&&!(physical_leaf(o,REVIEW_GPA)&2));
  assert((review_tables[3][15]&0x300)==0x100);
 }
 puts("PASS review N42A old trace/AD stamp/cache generation rejected before stale pointer traversal and no old D resurrection");
}

struct mr_pause_state {pthread_mutex_t mutex;pthread_cond_t cond;bool armed,held,release;unsigned position;};
static struct mr_pause_state mr_pause;
static void mr_unlock(void *p){
 struct review_owner *o=p;leave(p);
 if(!mr_pause.armed||o->id)return;
 if(mr_pause.position==2){if(!o->atomic.entry_active)return;}
 else{
  if(!o->atomic.fault.active||!o->atomic.fault.admitted)return;
  bool early=!o->atomic.fault.permit.active;if((mr_pause.position==0)!=early)return;
 }
 assert(!pthread_mutex_lock(&mr_pause.mutex));
 if(!mr_pause.held){
  mr_pause.held=true;assert(!pthread_cond_broadcast(&mr_pause.cond));
  while(!mr_pause.release)assert(!pthread_cond_wait(&mr_pause.cond,&mr_pause.mutex));
 }
 assert(!pthread_mutex_unlock(&mr_pause.mutex));
}
static void *mr_fault_thread(void *p){
 struct review_owner *o=p;physical_cpu=o->id;(void)fault(o,2,TR_ATOMIC_PROMOTED);return NULL;
}
static void mr_admission_race(void){
 for(unsigned position=0;position<2;position++)for(unsigned round=0;round<6;round++){
  struct mr_fixture *r=mr_new();r->read_only_owner0=true;mr_populate(r);
  memset(&mr_pause,0,sizeof(mr_pause));mr_pause.armed=true;mr_pause.position=position;
  assert(!pthread_mutex_init(&mr_pause.mutex,NULL)&&!pthread_cond_init(&mr_pause.cond,NULL));
  for(unsigned cpu=0;cpu<2;cpu++)r->f->owners[cpu].atomic.config.unlock=mr_unlock;
  pthread_t thread;assert(!pthread_create(&thread,NULL,mr_fault_thread,&r->f->owners[0]));
  assert(!pthread_mutex_lock(&mr_pause.mutex));
  while(!mr_pause.held)assert(!pthread_cond_wait(&mr_pause.cond,&mr_pause.mutex));
  assert(!pthread_mutex_unlock(&mr_pause.mutex));
  physical_cpu=1;assert(tam_close(&r->m,&r->ticket));
  assert(tam_drain(&r->m,r->ticket)==TAM_DRAIN_WAIT);
  assert(ad_updater_gate_open(&r->f->gate,r->f->stamp));
  assert(r->f->directory.operations==1&&r->f->owners[0].atomic.fault.active);
  assert(r->f->gate.references==(position?1u:0u));
  assert(!tam_revoke(&r->m,1,r->ticket)&&!tam_clear(&r->m,r->ticket));
  assert(!pthread_mutex_lock(&mr_pause.mutex));mr_pause.release=true;
  assert(!pthread_cond_broadcast(&mr_pause.cond)&&!pthread_mutex_unlock(&mr_pause.mutex));
  assert(!pthread_join(thread,NULL));mr_pause.armed=false;
  for(unsigned cpu=0;cpu<2;cpu++)r->f->owners[cpu].atomic.config.unlock=leave;
  assert(!r->f->owners[0].atomic.poisoned&&!r->f->directory.operations&&!r->f->gate.references);
  assert(tam_drain(&r->m,r->ticket)==TAM_DRAIN_READY);
  mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);assert(tam_finish(&r->m,r->ticket));
  mr_clean(r);assert(!pthread_cond_destroy(&mr_pause.cond)&&!pthread_mutex_destroy(&mr_pause.mutex));
 }
 puts("PASS review N42A 12 real-thread whole-fault/close schedules before and after updater acquisition");
}


static void *mr_entry_thread(void *p){
 struct review_owner *o=p;physical_cpu=o->id;
 assert(!tr_atomic_entry(&o->atomic,o->registry.active,o->registry.active->tree.eptp));return NULL;
}
static void mr_entry_close_race(void){
 struct mr_fixture *r=mr_new();mr_populate(r);memset(&mr_pause,0,sizeof(mr_pause));
 mr_pause.armed=true;mr_pause.position=2;
 assert(!pthread_mutex_init(&mr_pause.mutex,NULL)&&!pthread_cond_init(&mr_pause.cond,NULL));
 for(unsigned c=0;c<2;c++)r->f->owners[c].atomic.config.unlock=mr_unlock;
 pthread_t thread;assert(!pthread_create(&thread,NULL,mr_entry_thread,&r->f->owners[0]));
 assert(!pthread_mutex_lock(&mr_pause.mutex));
 while(!mr_pause.held)assert(!pthread_cond_wait(&mr_pause.cond,&mr_pause.mutex));
 assert(!pthread_mutex_unlock(&mr_pause.mutex));
 physical_cpu=1;assert(tam_close(&r->m,&r->ticket));
 assert(tam_drain(&r->m,r->ticket)==TAM_DRAIN_WAIT);
 assert(!pthread_mutex_lock(&mr_pause.mutex));mr_pause.release=true;
 assert(!pthread_cond_broadcast(&mr_pause.cond)&&!pthread_mutex_unlock(&mr_pause.mutex));
 assert(!pthread_join(thread,NULL));mr_pause.armed=false;
 for(unsigned c=0;c<2;c++)r->f->owners[c].atomic.config.unlock=leave;
 assert(r->f->owners[0].atomic.poisoned&&r->f->lease.poisoned&&r->entry_closed&&!r->opens);
 assert(!r->f->directory.operations&&tam_drain(&r->m,r->ticket)==TAM_DRAIN_FAILED);
 assert(!tam_clear(&r->m,r->ticket)&&!tam_finish(&r->m,r->ticket));tam_abort(&r->m);
 assert(r->m.state==TAM_POISONED&&r->f->owners[0].pool.live&&r->f->owners[1].pool.live);
 assert(!pthread_cond_destroy(&mr_pause.cond)&&!pthread_mutex_destroy(&mr_pause.mutex));
 puts("PASS review N42A already-admitted entry/close race fails closed, retains ownership, never authorizes physical entry");
}
struct mr_parallel_call {struct mr_fixture *r;unsigned cpu,stage;pthread_barrier_t *barrier;};
static void *mr_parallel_thread(void *p){
 struct mr_parallel_call *a=p;physical_cpu=a->cpu;
 int v=pthread_barrier_wait(a->barrier);assert(v==0||v==PTHREAD_BARRIER_SERIAL_THREAD);
 if(a->stage==0)assert(tam_revoke(&a->r->m,a->cpu,a->r->ticket));
 if(a->stage==1)assert(tam_reset(&a->r->m,a->cpu,a->r->ticket));
 if(a->stage==2)assert(tam_prepare(&a->r->m,a->cpu,a->r->ticket));
 return NULL;
}
static void mr_parallel_owners(void){
 for(unsigned round=0;round<8;round++){
  struct mr_fixture *r=mr_new();mr_populate(r);mr_close_drain(r);
  for(unsigned stage=0;stage<3;stage++){
   pthread_barrier_t barrier;assert(!pthread_barrier_init(&barrier,NULL,2));
   struct mr_parallel_call calls[2]={{r,0,stage,&barrier},{r,1,stage,&barrier}};pthread_t threads[2];
   for(unsigned c=0;c<2;c++)assert(!pthread_create(&threads[c],NULL,mr_parallel_thread,&calls[c]));
   for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],NULL));
   assert(!pthread_barrier_destroy(&barrier));physical_cpu=0;
   if(stage==0)mr_clear(r);
  }
  assert(tam_finish(&r->m,r->ticket));mr_clean(r);
 }
 puts("PASS review N42A eight real-thread all-owner revoke/reset/prepare interleavings with exact per-root receipts");
}

int main(int argc,char **argv){
 const char *test=argc>1?argv[1]:"all";
 #define RUN(name) if(!strcmp(test,"all")||!strcmp(test,#name))name()
 RUN(mr_init_storage);RUN(mr_cycle);RUN(mr_all_resident_retrack);RUN(mr_receipt_failures);RUN(mr_order_and_tickets);RUN(mr_admission_race);RUN(mr_failure_retention);RUN(mr_source_structure);RUN(mr_final_authority);RUN(mr_final_source_identity);RUN(mr_stale_trace);RUN(mr_reopen_probe);RUN(mr_entry_close_race);RUN(mr_parallel_owners);
 return 0;
}
