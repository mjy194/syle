/* Actual N42A coordinator/bridge/contexts/pools. Physical reference operations
 * and INVEPT are explicit mocks; this test makes no VMX claim. */
#define main n41b_base_main
#include "typed_root_atomic_test.c"
#undef main
#include "typed_root_atomic_maintenance.h"
struct maintenance_fixture {
 struct fixture f;
 struct tr_atomic_maintenance maintenance;
 struct tam_ticket ticket;
 uint64_t references[2];
 unsigned removed,restored,acks,loads,cas,fail_cas,fail_ack,corrupt_ack;
 bool closed,fail_remove,fail_restore,compete,competed,break_guard;
};
static bool mauthority(void *p,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *out) {
 struct owner *o=p;assert(!o->f->depth);o->authorities++;
 uint64_t off=gpa-s->gpa;
 *out=(struct nrc_atomic_page){.backing={s->object,s->lifetime,s->hpa+off,
  s->table_view?(const uint64_t *)((uintptr_t)s->table_view+off):NULL,s->permissions,6,true},
  .atomic_view=s->table_view?(uint64_t *)((uintptr_t)s->table_view+off):NULL,.pinned_writable=s->role==NRC_TABLE};return true;
}
static bool mclose(void *p,struct tam_ticket t) {
 struct maintenance_fixture *f=p;assert(!f->f.depth&&!f->closed&&t.nonce);
 f->closed=true;return true;
}
static bool mstopped(void *p,unsigned c,struct tam_ticket t) {
 struct maintenance_fixture *f=p;assert(!f->f.depth&&c<2&&t.nonce);return f->closed&&f->f.owners[c].stopped&&!f->break_guard;
}
static bool mremove(void *p,unsigned c,struct tam_ticket t) {
 struct maintenance_fixture *f=p;assert(mstopped(p,c,t));
 assert(f->references[c]==f->f.owners[c].registry.active->tree.eptp);
 if(f->fail_remove)return false;
 f->references[c]=0;f->removed++;return true;
}
static bool mflush(void *p,unsigned c,struct tam_ticket t,const struct tam_root *r,struct tam_flush_receipt *out) {
 struct maintenance_fixture *f=p;assert(mstopped(p,c,t)&&!f->references[c]&&!r->item->pins);
 assert(r->item->tree.generation==r->old_generation&&f->f.owners[c].pool.live>0);
 f->acks++;if(f->acks==f->fail_ack)return false;
 *out=(struct tam_flush_receipt){t,r->identity,r->full_eptp,r->hpa,r->history,r->old_generation,r->new_generation,UINT64_C(1)<<c,c,true};
 switch(f->corrupt_ack) {
 case 1:out->ticket.nonce++;break;case 2:out->full_eptp^=0x40;break;
 case 3:out->old_generation++;break;case 4:out->new_generation++;break;
 case 5:out->identity.allocator_lifetime++;break;case 6:out->cpu^=1;break;
 case 7:out->acked=0;break;case 8:out->history^=3;break;case 9:out->hpa+=4096;break;
 }
 return true;
}
static bool mrestore(void *p,unsigned c,struct tam_ticket t,const struct tam_root *r) {
 struct maintenance_fixture *f=p;assert(mstopped(p,c,t)&&r->item->pins==1&&r->binding->slot->directory.record.pins==1);
 if(f->fail_restore)return false;
 f->references[c]=r->full_eptp;f->restored++;return true;
}
static bool mopen(void *p,struct tam_ticket t) {
 struct maintenance_fixture *f=p;assert(!f->f.depth&&f->closed&&t.nonce&&f->references[0]&&f->references[1]);
 assert(f->maintenance.state==TAM_REOPENING&&f->f.gate.stamp.epoch==t.stamp.epoch+1);
 f->closed=false;return true;
}
static uint64_t mload(void *p,uint64_t *word) {struct maintenance_fixture *f=p;f->loads++;return __atomic_load_n(word,__ATOMIC_ACQUIRE);}
static bool mcas(void *p,uint64_t *word,uint64_t old,uint64_t value) {
 struct maintenance_fixture *f=p;f->cas++;
 if(f->fail_cas&&f->cas>=f->fail_cas)return false;
 if(f->compete&&!f->competed){f->competed=true;__atomic_fetch_and(word,~UINT64_C(0x100),__ATOMIC_ACQ_REL);}
 bool ok=__atomic_compare_exchange_n(word,&old,value,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 return ok;
}
static void mzero(const void *p,size_t n) {
 const unsigned char *bytes=p;for(size_t i=0;i<n;i++)assert(!bytes[i]);
}
static struct maintenance_fixture *msetup(void) {
 struct maintenance_fixture *f=calloc(1,sizeof *f);assert(f);setup(&f->f);f->f.catalog.atomic_authority=mauthority;
 const struct tam_hooks hooks={mclose,mstopped,mremove,mflush,mrestore,mopen,mload,mcas};
 /* Initialization must clear every inactive field, not rely on fresh BSS. */
 memset(&f->maintenance,0xa5,sizeof f->maintenance);f->maintenance.state=TAM_UNINITIALIZED;
 assert(tam_init(&f->maintenance,&f->f.owners[0].atomic,&f->f.owners[1].atomic,&hooks,f));
 struct tr_atomic_maintenance *m=&f->maintenance;
 assert(m->owners[0]==&f->f.owners[0].atomic&&m->owners[1]==&f->f.owners[1].atomic&&m->context==f);
 assert(m->state==TAM_OPEN&&m->type_epoch==f->f.lease.type_epoch&&m->source_count==f->f.catalog.root_count);
 assert(!memcmp(&m->hooks,&hooks,sizeof hooks));
 assert(!m->next_nonce&&!m->revoked&&!m->reset&&!m->prepared&&!m->changed&&!m->cleared_entries&&!m->drain_polls&&!m->page_count);
 mzero(&m->ticket,sizeof m->ticket);mzero(&m->next_stamp,sizeof m->next_stamp);
 mzero(m->roots,sizeof m->roots);mzero(m->receipts,sizeof m->receipts);mzero(m->root_count,sizeof m->root_count);
 mzero(m->pages,sizeof m->pages);mzero(m->operations,sizeof m->operations);
 mzero(m->operation_nonce,sizeof m->operation_nonce);mzero(m->owner_busy,sizeof m->owner_busy);
 for(size_t n=0;n<m->source_count;n++)assert(!memcmp(&m->sources[n],&f->f.catalog.roots[n],sizeof m->sources[n]));
 mzero(m->sources+m->source_count,(TAM_PAGES-m->source_count)*sizeof *m->sources);
 for(unsigned c=0;c<2;c++) {
  struct owner *o=&f->f.owners[c];select_root(o,0);read_write(o,true);
  select_root(o,c?2:1);read_write(o,true);
  struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);assert(item);
  f->references[c]=item->tree.eptp;assert(tr_atomic_entry(&o->atomic,item,item->tree.eptp));
 }
 return f;
}
static void mclosed(struct maintenance_fixture *f) {
 assert(tam_close(&f->maintenance,&f->ticket));
 assert(tam_drain(&f->maintenance,f->ticket)==TAM_DRAIN_READY);
 for(unsigned c=0;c<2;c++){f->f.cpu=c;assert(tam_revoke(&f->maintenance,c,f->ticket));}
 assert(f->maintenance.revoked==3&&f->f.catalog.references==4&&!f->f.catalog.closed);
}
static void mcleared(struct maintenance_fixture *f) {mclosed(f);assert(tam_clear(&f->maintenance,f->ticket));}
static void mreset(struct maintenance_fixture *f) {
 mcleared(f);for(unsigned c=0;c<2;c++){f->f.cpu=c;assert(tam_reset(&f->maintenance,c,f->ticket));}
}
static void successful(void) {
 struct maintenance_fixture *f=msetup();f->compete=true;
 uint64_t life[2],eptp[2];for(unsigned c=0;c<2;c++){life[c]=active(&f->f.owners[c])->directory.identity.allocator_lifetime;eptp[c]=f->references[c];}
 mreset(f);assert(f->competed&&f->maintenance.page_count==9&&f->maintenance.changed&&f->maintenance.cleared_entries==9);
 assert(!(__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300));
 assert(!(__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)&0x300));
 assert(f->acks==4&&f->f.gate.stamp.epoch==5&&f->f.lease.type_epoch==1);
 for(unsigned c=0;c<2;c++){f->f.cpu=c;assert(tam_prepare(&f->maintenance,c,f->ticket));}
 assert(tam_finish(&f->maintenance,f->ticket)&&f->f.gate.stamp.epoch==6);
 for(unsigned c=0;c<2;c++) {
  struct owner *o=&f->f.owners[c];f->f.cpu=c;
  assert(active(o)->directory.identity.allocator_lifetime==life[c]&&o->registry.active->tree.eptp==eptp[c]);
  assert(tr_atomic_entry(&o->atomic,o->registry.active,eptp[c]));
  read_write(o,false);
  uint64_t h=o->registry.active->tree.eptp&UINT64_C(0x000ffffffffff000),entry=0;
  for(unsigned l=0;l<4;l++) {
   uint64_t *page=NULL;for(unsigned n=0;n<PAGES;n++)if(o->pool_slots[n].live&&o->pages[n].hpa==h)page=o->pages[n].view;
   assert(page);entry=__atomic_load_n(&page[l==3?7:0],__ATOMIC_ACQUIRE);h=entry&UINT64_C(0x000ffffffffff000);
  }
  assert((entry&3)==1);
 }
 assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert((__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)&0x300)==0x100);
 for(unsigned c=0;c<2;c++) {
  struct owner *o=&f->f.owners[c];f->f.cpu=c;struct nested_ept_result out;
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,2,&out)==TR_ATOMIC_PROMOTED);
  for(unsigned n=0;n<2;n++)assert(o->traces[n].used&&!o->traces[n].registered&&!o->traces[n].usable);
 }
 assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x300);
 assert((__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)&0x300)==0x300);
 assert(f->f.roots[0].generation==10&&f->f.roots[0].lifetime==90&&f->f.lease.type_epoch==1);
 /* A second real round must revoke only fresh records, never revive old ones. */
 mreset(f);for(unsigned c=0;c<2;c++){f->f.cpu=c;assert(tam_prepare(&f->maintenance,c,f->ticket));}
 assert(tam_finish(&f->maintenance,f->ticket)&&f->f.gate.stamp.epoch==7);
 for(unsigned c=0;c<2;c++) {
  struct owner *o=&f->f.owners[c];f->f.cpu=c;f->references[c]=0;
  nested_ept_contexts_release_all_after_clear(&o->registry);
  assert(nested_ept_contexts_destroy(&o->registry)&&!o->pool.live&&o->allocations==o->frees);
 }
 assert(!f->f.catalog.references);free(f);
}
static void failures(void) {
 for(unsigned mode=0;mode<14;mode++) {
  struct maintenance_fixture *f=msetup();
  if(mode<9) {
   mcleared(f);f->corrupt_ack=mode+1;f->f.cpu=0;
   unsigned frees=f->f.owners[0].frees;size_t live=f->f.owners[0].pool.live;
   assert(!tam_reset(&f->maintenance,0,f->ticket));
   assert(f->f.owners[0].frees==frees&&f->f.owners[0].pool.live==live);
  }else if(mode==9){
   assert(tam_close(&f->maintenance,&f->ticket));assert(tam_drain(&f->maintenance,f->ticket)==TAM_DRAIN_READY);
   f->fail_remove=true;f->f.cpu=0;assert(!tam_revoke(&f->maintenance,0,f->ticket));
   assert(f->references[0]&&f->f.owners[0].registry.active->pins==1);
  }else if(mode==10){
   mreset(f);f->fail_restore=true;f->f.cpu=0;assert(!tam_prepare(&f->maintenance,0,f->ticket));
   assert(f->f.owners[0].registry.active->pins==1);
  }else if(mode==11){
   mclosed(f);f->fail_cas=2;assert(!tam_clear(&f->maintenance,f->ticket));assert(f->maintenance.changed==1);
  }else if(mode==12){
   mclosed(f);__atomic_fetch_or(&tables[0][0],0x200,__ATOMIC_ACQ_REL);
   assert(!tam_clear(&f->maintenance,f->ticket)&&!f->maintenance.changed);
  }else {
   mcleared(f);f->f.owners[0].injection=FLUSH_FAIL;f->f.cpu=0;
   unsigned frees=f->f.owners[0].frees;assert(!tam_reset(&f->maintenance,0,f->ticket));assert(f->f.owners[0].frees==frees&&!f->acks);
  }
  assert(f->maintenance.state==TAM_POISONED&&f->closed&&f->f.gate.fault&&f->f.catalog.references==4);
  assert(!tam_finish(&f->maintenance,f->ticket));free(f);
 }
}
static void drains_and_tickets(void) {
 struct maintenance_fixture *f=msetup();struct td_operation op={0};uint64_t nonce;
 lock(&f->f.owners[0]);assert(td_begin(&f->f.directory,&op,&nonce));unlock(&f->f.owners[0]);
 assert(tam_close(&f->maintenance,&f->ticket));
 assert(tam_drain(&f->maintenance,f->ticket)==TAM_DRAIN_WAIT&&!f->f.gate.closed);
 /* A pre-admitted operation may still acquire/release its updater. */
 struct ad_updater_token updater={0};uint64_t ticket;
 assert(ad_updater_gate_acquire(&f->f.gate,f->f.stamp,&updater,&ticket));
 lock(&f->f.owners[0]);assert(td_end(&f->f.directory,&op,nonce));unlock(&f->f.owners[0]);
 assert(tam_drain(&f->maintenance,f->ticket)==TAM_DRAIN_WAIT&&f->f.gate.closed);
 assert(ad_updater_gate_release(&f->f.gate,&updater,f->f.stamp,ticket));
 assert(tam_drain(&f->maintenance,f->ticket)==TAM_DRAIN_READY);
 struct tam_ticket wrong=f->ticket;wrong.nonce++;
 assert(!tam_clear(&f->maintenance,wrong)&&!tam_finish(&f->maintenance,f->ticket)&&f->maintenance.state==TAM_DRAINED);
 for(unsigned c=0;c<2;c++) {
  f->f.cpu=c;struct owner *o=&f->f.owners[c];struct nested_ept_result out;
  assert(nested_ept_contexts_select(&o->registry,o->registry.active->key)!=NESTED_CONTEXT_OK);
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x8000,1,&out)==TR_ATOMIC_REJECTED);
  assert(!o->atomic.poisoned);assert(tam_revoke(&f->maintenance,c,f->ticket));
  assert(!tam_revoke(&f->maintenance,c,f->ticket));
 }
 assert(tam_clear(&f->maintenance,f->ticket));f->f.cpu=0;assert(tam_reset(&f->maintenance,0,f->ticket));
 assert(!tam_prepare(&f->maintenance,0,f->ticket)&&!tam_finish(&f->maintenance,f->ticket));
 tam_abort(&f->maintenance);assert(f->maintenance.state==TAM_POISONED);free(f);
 f=msetup();assert(tam_close(&f->maintenance,&f->ticket));
 lock(&f->f.owners[0]);assert(td_begin(&f->f.directory,&op,&nonce));unlock(&f->f.owners[0]);
 f->maintenance.drain_polls=TAM_DRAIN_POLLS;assert(tam_drain(&f->maintenance,f->ticket)==TAM_DRAIN_FAILED&&f->maintenance.state==TAM_POISONED);free(f);
}
int main(void) {
 successful();failures();drains_and_tickets();
 puts("PASS N42A production two-owner AD clear/retrack: shared A/B, distinct C, guarded exact receipts, repeat round and poison retention");return 0;
}
