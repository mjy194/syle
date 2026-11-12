#include "nested_atomic_catalog.h"
#include "nested_ept_sparse.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static _Alignas(4096) uint64_t tables[9][512];
static const uint64_t gpas[9]={0x11000,0x27000,0x38000,0x50000,0x71000,0xc0000,0xc7000,0xdd000,0xf1000};
struct fixture {
 struct nested_root_catalog catalog;
 struct nrc_span spans[11];struct nrc_root roots[3];
 struct nrc_identity id;
 struct ad_updater_gate gate;struct ad_entry_stamp stamp;
};
struct owner {
 struct fixture *f;struct nac_permit permit;struct nac_ticket ticket;
 struct nac_cache_identity cache;
 unsigned calls,guards,corrupt,deny_guard;
 bool fail,cache_invalid,close_catalog,close_gate,concurrent,lock_source_on_guard;
 struct nested_ept_sparse tree;
 unsigned allocations,frees,flushes;
 bool flush_fail,check_flush;
};
static bool authority(void *context,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *out) {
 struct owner *o=context;struct fixture *f=o->f;
 if(!o->concurrent)assert(!f->catalog.lock&&!f->gate.lock);
 o->calls++;
 if(o->fail)return false;
 uint64_t off=gpa-s->gpa;
 *out=(struct nrc_atomic_page){.backing={s->object,s->lifetime,s->hpa+off,
  s->table_view?(const uint64_t *)((uintptr_t)s->table_view+off):NULL,s->permissions,6,true},
  .atomic_view=s->table_view?(uint64_t *)((uintptr_t)s->table_view+off):NULL,.pinned_writable=s->role==NRC_TABLE};
 switch(o->corrupt) {
 case 1:out->backing.object++;break;case 2:out->backing.lifetime++;break;
 case 3:out->backing.hpa+=4096;break;case 4:out->backing.permissions^=2;break;
 case 5:out->backing.memory_type=0;break;case 6:out->backing.ignore_pat=false;break;
 case 7:out->backing.table_view=NULL;break;case 8:out->atomic_view=tables[8];break;
 case 9:out->pinned_writable=false;break;
 }
 if(o->close_catalog){o->close_catalog=false;assert(nrc_close(&f->catalog,f->id));}
 if(o->close_gate){o->close_gate=false;assert(ad_updater_gate_close(&f->gate,f->stamp));}
 return true;
}
static bool cache_guard(void *context,struct nac_cache_identity c) {
 struct owner *o=context;o->guards++;
 if(!o->concurrent)assert(!o->f->catalog.lock&&!o->f->gate.lock);
 if(o->lock_source_on_guard){o->lock_source_on_guard=false;__atomic_store_n(&o->f->catalog.lock,1,__ATOMIC_RELEASE);return false;}
 return !o->cache_invalid&&o->guards!=o->deny_guard&&!o->tree.poisoned&&
  c.owner==o->cache.owner&&c.hpa==o->cache.hpa&&c.full_eptp==o->cache.full_eptp&&
  c.lifetime==o->cache.lifetime&&c.generation==o->cache.generation;
}
static void setup(struct fixture *f) {
 memset(f,0,sizeof *f);memset(tables,0,sizeof tables);
 f->id=(struct nrc_identity){1,2,3,4};f->stamp=(struct ad_entry_stamp){1,2,3,5,4};
 assert(nrc_init_atomic_wb(&f->catalog,f->id,48,f->spans,11,f->roots,3,authority));
 for(unsigned i=0;i<9;i++) {
  struct nrc_span s={.identity=f->id,.object=i+1,.lifetime=i+50,.gpa=gpas[i],.hpa=0x100000+i*4096,
   .bytes=4096,.table_view=tables[i],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
  assert(!nrc_add_span(&f->catalog,s));
  assert(!nrc_add_atomic_span(&f->catalog,s,NULL));
  assert(nrc_add_atomic_span(&f->catalog,s,tables[i]));
 }
 for(unsigned i=0;i<2;i++) {
  struct nrc_span s={.identity=f->id,.object=20+i,.lifetime=80+i,.gpa=0x200000+i*0x8000,
   .hpa=0x500000+i*0x4000,.bytes=4096,.permissions=3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
  struct nrc_span bad=s;bad.allowed_types|=NRC_TYPE_UC;
  assert(!nrc_add_atomic_span(&f->catalog,bad,NULL));
  assert(!nrc_add_atomic_span(&f->catalog,s,tables[0]));
  assert(nrc_add_atomic_span(&f->catalog,s,NULL));
 }
 const unsigned root_index[]={0,1,5};
 for(unsigned i=0;i<3;i++) {
  uint64_t root=gpas[root_index[i]];
  struct nrc_root r={f->id,30+i,i,10+i,90+i,root|0x5e,root},bad=r;
  bad.full_eptp=root|0x1e;assert(!nrc_add_root(&f->catalog,bad));
  assert(nrc_add_root(&f->catalog,r));
 }
 tables[0][0]=tables[1][0]=gpas[2]|7;tables[2][0]=gpas[3]|7;tables[3][0]=gpas[4]|7;
 tables[4][7]=0x200073;
 tables[5][0]=gpas[6]|7;tables[6][0]=gpas[7]|7;tables[7][0]=gpas[8]|7;tables[8][7]=0x208073;
 assert(nrc_publish(&f->catalog));assert(ad_updater_gate_init(&f->gate,f->stamp));
}
static void owner_init(struct owner *o,struct fixture *f,unsigned n) {
 *o=(struct owner){.f=f,.cache={n,0x900000+n*0x1000,(0x900000+n*0x1000)|0x5e,100+n,1}};
}
static bool begin(struct owner *o,unsigned root) {
 return nac_begin(&o->f->catalog,o->f->roots[root],o,&o->f->gate,o->f->stamp,
  o->cache,cache_guard,o,&o->permit,&o->ticket);
}
static struct nested_ept_atomic_result walk(struct owner *o,unsigned need,struct nac_trace *t) {
 return nac_walk(&o->permit,o->ticket,0x7000,need,t);
}
static void basic(void) {
 struct fixture f;setup(&f);struct owner o;owner_init(&o,&f,0);
 for(unsigned i=0;i<3;i++) {
  assert(begin(&o,i));struct nac_trace t;struct nested_ept_atomic_result r=walk(&o,1,&t);
  assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&t.complete);
  assert(r.translation.hpa==(i==2?0x504000:0x500000)&&r.translation.memory_explicit&&r.translation.ignore_pat);
  assert(f.catalog.references==1&&f.gate.references==1&&o.permit.active);
  assert(nac_trace_guard(&o.permit,o.ticket,0x7000,&t));
  assert(!nac_trace_guard(&o.permit,o.ticket,0x7001,&t));
  struct nrc_page untouched={.hpa=123};unsigned calls=o.calls;
  assert(!nrc_resolve(&o.permit.source,o.ticket.source_nonce,gpas[0],1,&untouched)&&untouched.hpa==123);
  assert(nrc_walk(&o.permit.source,o.ticket.source_nonce,0x7000,1).status==NESTED_EPT_UNSUPPORTED&&o.calls==calls);
  assert(nac_complete(&o.permit,o.ticket)&&!o.permit.active);
 }
 assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert((__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)&0x300)==0x100);
}
static void bad_authority(void) {
 struct fixture f;setup(&f);struct owner o;owner_init(&o,&f,0);assert(begin(&o,0));
 for(unsigned i=1;i<=9;i++) {
  o.corrupt=i;struct nrc_atomic_page page={.backing={.hpa=123}};
  assert(!nrc_resolve_atomic(&o.permit.source,o.ticket.source_nonce,gpas[0],1,&page)&&page.backing.hpa==123);
  struct nac_trace t;struct nested_ept_atomic_result r=walk(&o,1,&t);
  assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);
 }
 o.corrupt=0;assert(nac_complete(&o.permit,o.ticket));
}
static void stale(void) {
 struct fixture f;setup(&f);struct owner o;owner_init(&o,&f,0);assert(begin(&o,0));
 struct nac_trace t;assert(walk(&o,1,&t).status==NEA_DONE);struct nac_ticket old=o.ticket;
 assert(nac_complete(&o.permit,o.ticket));assert(begin(&o,0));
 assert(!nac_guard(&o.permit,old)&&!nac_end(&o.permit,old));
 assert(nac_trace_guard(&o.permit,o.ticket,0x7000,&t)); /* fresh permit, same durable identity */
 /* Corrupt every durable identity. Poisoned pointer must never be followed. */
 for(unsigned i=0;i<21;i++) {
  struct nac_trace bad=t;bad.path.entry[0].pointer=(uint64_t *)(uintptr_t)1;
  switch(i) {
  case 0:bad.source.identity.domain++;break;case 1:bad.source.identity.incarnation++;break;
  case 2:bad.source.identity.catalog++;break;case 3:bad.source.identity.lifetime++;break;
  case 4:bad.source.source++;break;case 5:bad.source.owner++;break;case 6:bad.source.generation++;break;
  case 7:bad.source.lifetime++;break;case 8:bad.source.full_eptp^=0x40;break;case 9:bad.source.root_gpa+=4096;break;
  case 10:bad.cache.owner++;break;case 11:bad.cache.hpa+=4096;break;case 12:bad.cache.full_eptp+=4096;break;
  case 13:bad.cache.lifetime++;break;case 14:bad.cache.generation++;break;
  case 15:bad.path.stamp.domain_id++;break;case 16:bad.path.stamp.incarnation++;break;
  case 17:bad.path.stamp.context_id++;break;case 18:bad.path.stamp.epoch++;break;
  case 19:bad.path.stamp.lifetime++;break;case 20:bad.gpa++;break;
  }
  unsigned calls=o.calls;
  assert(!nac_trace_guard(&o.permit,o.ticket,0x7000,&bad)&&o.calls==calls);
 }
 struct nac_trace bad=t;bad.path.entry[3].table_hpa+=4096;
 assert(!nac_trace_guard(&o.permit,o.ticket,0x7000,&bad));
 bad=t;bad.path.entry[3].entry_gpa+=8;assert(!nac_trace_guard(&o.permit,o.ticket,0x7000,&bad));
 bad=t;bad.translation.hpa+=4096;assert(!nac_trace_guard(&o.permit,o.ticket,0x7000,&bad));
 __atomic_fetch_xor(&tables[4][7],0x8000,__ATOMIC_ACQ_REL);
 assert(!nac_trace_guard(&o.permit,o.ticket,0x7000,&t));
 assert(!( __atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x200)); /* no CAS on failure */
 assert(nac_complete(&o.permit,o.ticket));
 assert(begin(&o,1));assert(!nac_trace_guard(&o.permit,o.ticket,0x7000,&t));assert(nac_complete(&o.permit,o.ticket));
}
static void drain_and_retention(void) {
 struct fixture f;setup(&f);struct owner a,b;owner_init(&a,&f,0);owner_init(&b,&f,1);
 assert(begin(&a,0));a.close_catalog=true;a.close_gate=true;struct nac_trace t;
 assert(walk(&a,2,&t).status==NEA_DONE);
 assert(!begin(&b,1)&&!b.permit.active);
 assert(!nrc_drained(&f.catalog,f.id)&&!ad_updater_gate_drained(&f.gate,f.stamp));
 assert(nac_complete(&a.permit,a.ticket));
 assert(nrc_drained(&f.catalog,f.id)&&ad_updater_gate_drained(&f.gate,f.stamp));
 /* Terminal source close is NEVER used for maintenance reopen. */
 struct ad_entry_stamp next;assert(ad_updater_gate_reopen(&f.gate,f.stamp,true,&next));f.stamp=next;
 assert(!begin(&a,0));
 setup(&f);owner_init(&a,&f,0);assert(begin(&a,0));
 __atomic_store_n(&f.gate.lock,1,__ATOMIC_RELEASE);
 assert(!nac_end(&a.permit,a.ticket)&&a.permit.active&&a.permit.source.live&&a.permit.updater.live);
 assert(f.catalog.references==1&&f.gate.references==1&&f.catalog.fault&&f.gate.fault);
 __atomic_store_n(&f.gate.lock,0,__ATOMIC_RELEASE);
 assert(nac_end(&a.permit,a.ticket)&&!a.permit.active&&!f.catalog.references&&!f.gate.references);
 assert(!begin(&a,0));
 setup(&f);owner_init(&a,&f,0);assert(begin(&a,0));
 __atomic_store_n(&f.catalog.lock,1,__ATOMIC_RELEASE);
 assert(!nac_end(&a.permit,a.ticket)&&a.permit.active&&a.permit.source.live&&!a.permit.updater.live);
 assert(f.catalog.references==1&&!f.gate.references&&f.gate.fault);
 __atomic_store_n(&f.catalog.lock,0,__ATOMIC_RELEASE);assert(nac_end(&a.permit,a.ticket));
 setup(&f);owner_init(&a,&f,0);a.lock_source_on_guard=true;
 assert(!begin(&a,0)&&a.permit.active&&a.permit.source.live&&!a.permit.updater.live);
 assert(a.ticket.source_nonce&&a.ticket.updater_nonce&&f.catalog.references==1&&f.gate.fault);
 __atomic_store_n(&f.catalog.lock,0,__ATOMIC_RELEASE);assert(nac_end(&a.permit,a.ticket));
 setup(&f);owner_init(&a,&f,0);f.gate.next_nonce=UINT64_MAX;
 assert(!begin(&a,0)&&!a.permit.active&&!f.catalog.references&&!f.gate.references);
 setup(&f);owner_init(&a,&f,0);a.cache_invalid=true;
 assert(!begin(&a,0)&&!a.permit.active&&!f.catalog.references&&!f.gate.references);
}
static void guard_seams(void) {
 struct fixture f;setup(&f);struct owner o;owner_init(&o,&f,0);assert(begin(&o,0));
 o.guards=0;struct nac_trace t;assert(walk(&o,2,&t).status==NEA_DONE);unsigned total=o.guards;
 assert(nac_complete(&o.permit,o.ticket));bool partial=false;
 for(unsigned i=1;i<=total;i++) {
  setup(&f);owner_init(&o,&f,0);assert(begin(&o,0));o.guards=0;o.deny_guard=i;
  struct nested_ept_atomic_result r=walk(&o,2,&t);
  assert(r.status==NEA_STALE&&!t.complete&&o.permit.active&&f.catalog.references==1&&f.gate.references==1);
  partial|=r.changed;o.deny_guard=0;assert(nac_end(&o.permit,o.ticket));
 }
 assert(partial);
}
static bool alloc(void *context,struct nested_sparse_page *out) {
 struct owner *o=context;void *p=aligned_alloc(4096,4096);if(!p)return false;
 *out=(struct nested_sparse_page){0x1000000+o->allocations++*4096,p};return true;
}
static void release(void *context,struct nested_sparse_page page) {struct owner *o=context;o->frees++;free(page.view);}
static bool flush(void *context,uint64_t eptp) {
 struct owner *o=context;assert((eptp&4095)==0x5e);o->flushes++;
 if(o->check_flush)assert(nac_guard(&o->permit,o->ticket)&&o->f->catalog.references&&o->f->gate.references);
 return !o->flush_fail;
}
static void cache_work(bool fail_flush) {
 struct fixture f;setup(&f);struct owner o;owner_init(&o,&f,0);
 const struct nested_sparse_ops ops={alloc,release,flush};
 assert(nested_ept_sparse_init_mode(&o.tree,&ops,&o,48,true));
 o.cache.hpa=o.tree.eptp&~UINT64_C(4095);o.cache.full_eptp=o.tree.eptp;
 __atomic_fetch_or(&tables[4][7],0x200,__ATOMIC_ACQ_REL); /* peer already dirty */
 assert(begin(&o,0));o.check_flush=true;struct nac_trace t;
 struct nested_ept_atomic_result r=walk(&o,1,&t);assert(r.status==NEA_DONE);
 assert(nested_ept_sparse_fill_tracked(&o.tree,0x7000,&r.translation,true)==NESTED_SPARSE_OK);
 bool tracked=false;assert(nested_ept_sparse_query_tracking(&o.tree,0x7000,&tracked)&&tracked);
 assert(nac_complete(&o.permit,o.ticket));
 assert(begin(&o,0));assert(nac_trace_guard(&o.permit,o.ticket,0x7000,&t));
 struct nac_trace write;assert(walk(&o,2,&write).status==NEA_DONE);
 o.flush_fail=fail_flush;
 assert(nested_ept_sparse_promote_write(&o.tree,0x7000)==!fail_flush);
 if(fail_flush) {
  assert(o.tree.poisoned&&o.tree.root&&o.allocations>o.frees);
  assert(!nac_complete(&o.permit,o.ticket));
 }else {
  assert(nac_complete(&o.permit,o.ticket));
  assert(nested_ept_sparse_query_tracking(&o.tree,0x7000,&tracked)&&!tracked);
 }
 o.check_flush=false;o.flush_fail=false;
 assert(nested_ept_sparse_destroy(&o.tree));assert(o.allocations==o.frees);
}
struct thread {struct owner owner;unsigned root,need;uint32_t *ready,*go;struct nac_trace trace;};
static void *run(void *context) {
 struct thread *t=context;assert(begin(&t->owner,t->root));
 __atomic_fetch_add(t->ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(t->go,__ATOMIC_ACQUIRE))sched_yield();
 assert(walk(&t->owner,t->need,&t->trace).status==NEA_DONE);
 assert(nac_complete(&t->owner.permit,t->owner.ticket));return NULL;
}
static void concurrent(void) {
 for(unsigned round=0;round<20;round++) {
  struct fixture f;setup(&f);uint32_t ready=0,go=0;struct thread a={.root=0,.need=1,.ready=&ready,.go=&go};
  struct thread b={.root=1,.need=2,.ready=&ready,.go=&go};owner_init(&a.owner,&f,0);owner_init(&b.owner,&f,1);a.owner.concurrent=b.owner.concurrent=true;
  pthread_t ta,tb;assert(!pthread_create(&ta,NULL,run,&a)&&!pthread_create(&tb,NULL,run,&b));
  while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=2)sched_yield();
  assert(ad_updater_gate_close(&f.gate,f.stamp)&&!ad_updater_gate_drained(&f.gate,f.stamp));
  __atomic_store_n(&go,1,__ATOMIC_RELEASE);assert(!pthread_join(ta,NULL)&&!pthread_join(tb,NULL));
  assert(ad_updater_gate_drained(&f.gate,f.stamp)&&nrc_idle(&f.catalog,f.id));
  assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x300);
  assert((__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)&0x300)==0);
  /* No physical consumers here. Test-only stopped/invalidated attestation. */
  for(unsigned i=0;i<9;i++)for(unsigned j=0;j<512;j++)__atomic_fetch_and(&tables[i][j],~UINT64_C(0x300),__ATOMIC_ACQ_REL);
  struct ad_entry_stamp next;assert(ad_updater_gate_reopen(&f.gate,f.stamp,true,&next));f.stamp=next;
  assert(begin(&a.owner,0));assert(!nac_trace_guard(&a.owner.permit,a.owner.ticket,0x7000,&a.trace));
  struct nac_trace t;assert(walk(&a.owner,1,&t).status==NEA_DONE);
  assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x100);
  assert(nac_complete(&a.owner.permit,a.owner.ticket));
 }
}
int main(void) {
 basic();bad_authority();stale();drain_and_retention();guard_seams();cache_work(false);cache_work(true);concurrent();
 puts("PASS N41A atomic source authority, distinct/shared roots, full permits/traces, tracked fills and retained failures (pure)");
}
