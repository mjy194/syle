/* Actual shared catalog, distinct caller-stable sessions, concurrent close. */
#include "nested_root_catalog.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#define THREADS 8u
static _Alignas(4096) uint64_t pages[4][512];
struct run {
 struct nested_root_catalog *catalog;
 struct nrc_root root;
 struct nrc_session session;
 uint64_t nonce;
 unsigned calls;
 bool require_admission,accepted;
};
static unsigned ready,go,start;
static bool authority(void *context,const struct nrc_span *s,uint64_t gpa,struct nrc_page *out) {
 struct run *r=context;r->calls++;
 /* Reenter metadata check: this cannot succeed if our callback holds its lock. */
 assert(nrc_guard(&r->session,r->nonce));
 uint64_t off=gpa-s->gpa;
 *out=(struct nrc_page){s->object,s->lifetime,s->hpa+off,
  s->table_view?(void *)((uintptr_t)s->table_view+off):NULL,s->permissions,6,true};
 return true;
}
static void *worker(void *opaque) {
 struct run *r=opaque;
 while(!__atomic_load_n(&start,__ATOMIC_ACQUIRE))sched_yield();
 r->accepted=nrc_begin(r->catalog,r->root,r,&r->session,&r->nonce);
 if(r->require_admission)assert(r->accepted);
 __atomic_fetch_add(&ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))sched_yield();
 if(r->accepted){
  struct nested_ept_result out=nrc_walk(&r->session,r->nonce,0x5000,3);
  assert(out.status==NESTED_EPT_OK&&out.hpa==0x800000);
  assert(nrc_end(&r->session,r->nonce));
 }
 return NULL;
}
static void schedule(bool race) {
 struct nested_root_catalog c={0};struct nrc_span spans[2];struct nrc_root roots[1];
 static uint64_t lifetime=103;struct nrc_identity id={100,101,102,++lifetime};
 assert(nrc_init(&c,id,48,spans,2,roots,1,authority));
 struct nrc_span table={.identity=id,.object=1,.lifetime=1,.gpa=0x10000,.hpa=0x100000,
  .bytes=16384,.table_view=pages[0],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
 struct nrc_span data={.identity=id,.object=2,.lifetime=2,.gpa=0x40000,.hpa=0x800000,
  .bytes=4096,.permissions=3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
 assert(nrc_add_span(&c,table)&&nrc_add_span(&c,data));
 struct nrc_root root={id,1,0,1,1,0x1001e,0x10000};
 assert(nrc_add_root(&c,root));
 memset(pages,0,sizeof(pages));for(unsigned i=0;i<3;i++)pages[i][0]=(0x11000+i*4096)|7;
 pages[3][5]=0x40073;assert(nrc_publish(&c));
 struct run runs[THREADS]={0};pthread_t threads[THREADS];ready=go=start=0;
 for(unsigned i=0;i<THREADS;i++){
  runs[i]=(struct run){.catalog=&c,.root=root,.require_admission=!race};
  assert(!pthread_create(&threads[i],NULL,worker,&runs[i]));
 }
 __atomic_store_n(&start,1,__ATOMIC_RELEASE);
 if(!race)while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=THREADS)sched_yield();
 assert(nrc_close(&c,id));
 struct nrc_session late={0};uint64_t nonce=0;
 assert(!nrc_begin(&c,root,NULL,&late,&nonce));
 if(!race)assert(!nrc_drained(&c,id));
 __atomic_store_n(&go,1,__ATOMIC_RELEASE);
 for(unsigned i=0;i<THREADS;i++){
  assert(!pthread_join(threads[i],NULL));
  assert(!runs[i].session.live);
  if(runs[i].accepted)assert(runs[i].calls==5);
 }
 assert(nrc_drained(&c,id)&&!c.fault);
}
int main(void) {
 for(unsigned i=0;i<50;i++){schedule(false);schedule(true);}
 puts("PASS independent N40A 100 real eight-thread close/admission/drain schedules");
}
