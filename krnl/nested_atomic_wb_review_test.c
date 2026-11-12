/* Independent N41A production walker review. No VMX or physical proof. */
#define _GNU_SOURCE
#include "nested_ept_atomic.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static _Alignas(4096) uint64_t pages[4][512];
static struct ad_entry_stamp stamp={11,12,13,14,15};
struct fixture {
 unsigned tables[4],loads,compares,guards,deny_guard,wrong_hpa;
 unsigned collision,clear_dirty,deny_leaf,reject_cas,structural;
 uint8_t permissions;
 pthread_barrier_t *barrier;
 unsigned *collision_count;
};
static bool guard(void *p,struct ad_entry_stamp s) {
 struct fixture *f=p;
 assert(!memcmp(&s,&stamp,sizeof(s)));
 return ++f->guards!=f->deny_guard;
}
static bool table(void *p,uint64_t gpa,uint64_t *hpa,uint64_t **view) {
 struct fixture *f=p;
 if(gpa<0x1000||gpa>0x4000||(gpa&4095))return false;
 unsigned n=(unsigned)(gpa/4096-1);
 f->tables[n]++;
 *hpa=0x100000+n*4096;*view=pages[n];
 if(f->wrong_hpa==n+1&&f->tables[n]>1)*hpa+=0x10000;
 return true;
}
static bool leaf(void *p,uint64_t gpa,uint64_t *hpa,uint8_t *permissions) {
 struct fixture *f=p;
 assert(gpa==0x9000);
 *hpa=0x900000;*permissions=f->permissions;
 return !f->deny_leaf;
}
static uint64_t load(void *p,uint64_t *word) {
 struct fixture *f=p;
 f->loads++;
 if(f->clear_dirty&&f->loads==5)
  __atomic_fetch_and(&pages[3][15],~UINT64_C(0x200),__ATOMIC_ACQ_REL);
 return __atomic_load_n(word,__ATOMIC_ACQUIRE);
}
static bool cas(void *p,uint64_t *word,uint64_t expected,uint64_t desired) {
 struct fixture *f=p;f->compares++;
 if(f->reject_cas)return false;
 if(f->structural&&f->compares==1){
  __atomic_fetch_xor(word,UINT64_C(0x1000),__ATOMIC_ACQ_REL);return false;
 }
 if(f->barrier&&f->compares==1){
  int e=pthread_barrier_wait(f->barrier);assert(!e||e==PTHREAD_BARRIER_SERIAL_THREAD);
 }
 if(f->collision&&f->compares==1){
  __atomic_fetch_or(word,UINT64_C(0x100),__ATOMIC_ACQ_REL);return false;
 }
 bool ok=__atomic_compare_exchange_n(word,&expected,desired,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 if(!ok&&f->collision_count)__atomic_fetch_add(f->collision_count,1,__ATOMIC_ACQ_REL);
 return ok;
}
static const struct nested_ept_atomic_ops ops={guard,table,leaf,load,cas};
static void init(struct fixture *f) {
 *f=(struct fixture){.permissions=3};
 memset(pages,0,sizeof(pages));
 pages[0][0]=0x2007;pages[1][0]=0x3007;pages[2][0]=0x4007;
 pages[3][15]=0x9073;
}
static struct nested_ept_atomic_result walk(struct fixture *f,unsigned access,
 struct nested_ept_atomic_trace *trace) {
 return nested_ept_atomic_walk_wb_ipat(0x105e,0xf037,access,48,&ops,f,stamp,trace);
}
static void explicit_memory(void) {
 struct fixture f;struct nested_ept_atomic_trace t;init(&f);
 struct nested_ept_atomic_result r=walk(&f,3,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&r.changed);
 assert(r.translation.hpa==0x900037&&r.translation.l1_gpa==0x9037);
 assert(r.translation.effective12==3&&r.translation.effective02==3);
 assert(r.translation.memory_explicit&&r.translation.ignore_pat);
 assert(r.translation.leaf_type==6&&r.translation.walk_type==6);
 assert(pages[3][15]==0x9373&&t.count==4);
 init(&f);pages[3][15]&=~UINT64_C(0x40);
 r=walk(&f,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);
 r=nested_ept_atomic_walk(0x105e,0xf037,1,48,&ops,&f,stamp,&t);
 assert(r.status==NEA_UNSUPPORTED&&!r.changed); /* Legacy still rejects IPAT1. */
 pages[3][15]&=~UINT64_C(0x40);
 r=nested_ept_atomic_walk(0x105e,0xf037,1,48,&ops,&f,stamp,&t);
 assert(r.status==NEA_DONE&&!r.translation.memory_explicit);
 puts("PASS review explicit WB/IPAT1 outputs and unchanged legacy defaults");
}
static void format_and_faults(void) {
 struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
 for(unsigned mt=0;mt<8;mt++)if(mt!=6){
  init(&f);pages[3][15]=(pages[3][15]&~UINT64_C(0x38))|((uint64_t)mt<<3);
  r=walk(&f,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 }
 const uint64_t bad[]={0x80,0x400,0x800,UINT64_C(1)<<48,UINT64_C(1)<<63};
 for(unsigned level=0;level<4;level++)for(unsigned j=0;j<sizeof(bad)/sizeof(bad[0]);j++){
  init(&f);pages[level][level==3?15:0]|=bad[j];
  r=walk(&f,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 }
 for(unsigned level=0;level<3;level++){
  init(&f);pages[level][0]|=0x200;
  r=walk(&f,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 }
 for(unsigned level=0;level<4;level++){
  init(&f);pages[level][level==3?15:0]=UINT64_MAX&~UINT64_C(7);
  r=walk(&f,3,&t);
  assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED);
  assert(r.translation.effective12==0&&t.count==level&&r.changed==(level>0));
  for(unsigned prior=0;prior<level;prior++)assert(pages[prior][0]&0x100);
  assert(pages[level][level==3?15:0]==(UINT64_MAX&~UINT64_C(7)));
  for(unsigned later=level+1;later<4;later++)assert(!f.tables[later]);
 }
 for(unsigned level=0;level<4;level++){
  init(&f);pages[level][level==3?15:0]&=~UINT64_C(2);
  r=walk(&f,2,&t);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED);
  assert(!(pages[3][15]&0x200));
  init(&f);pages[level][level==3?15:0]&=~UINT64_C(2);f.deny_leaf=true;
  r=walk(&f,2,&t);assert(r.status==NEA_BACKING&&!r.changed);
 }
 init(&f);f.permissions=1;pages[3][15]&=~UINT64_C(2);
 r=walk(&f,2,&t);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);
 init(&f);pages[0][0]=0x2002;
 r=walk(&f,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);pages[0][0]=0x2004;
 r=walk(&f,4,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 puts("PASS review reserved formats, permission-zero short circuit and L0/L1 fault ownership");
}
static void partial_and_replay(void) {
 struct fixture f;struct nested_ept_atomic_trace t;init(&f);f.collision=1;
 struct nested_ept_atomic_result r=walk(&f,2,&t);
 assert(r.status==NEA_DONE&&r.changed&&f.compares==4&&pages[3][15]==0x9373);
 init(&f);pages[3][15]|=0x200;f.clear_dirty=1;
 r=walk(&f,1,&t);assert(r.status==NEA_DONE&&pages[3][15]==0x9173);
 init(&f);f.structural=1;
 r=walk(&f,2,&t);assert(r.status==NEA_REWALK&&!r.changed&&f.compares==1);
 init(&f);f.reject_cas=1;
 r=walk(&f,2,&t);assert(r.status==NEA_RETRY&&!r.changed&&f.compares==64);
 init(&f);f.wrong_hpa=3;
 r=walk(&f,2,&t);assert(r.status==NEA_BACKING&&r.changed);
 assert(pages[0][0]==0x2107&&pages[1][0]==0x3107);
 assert(pages[2][0]==0x4007&&pages[3][15]==0x9073);
 init(&f);r=walk(&f,2,&t);unsigned guards=f.guards;bool partial=false;
 for(unsigned i=1;i<=guards;i++){
  init(&f);f.deny_guard=i;r=walk(&f,2,&t);
  assert(r.status==NEA_STALE);partial|=r.changed;
 }
 assert(partial);
 puts("PASS review deterministic CAS collision, no remembered D replay and applied partial faults");
}
struct thread {struct fixture fixture;struct nested_ept_atomic_result result;unsigned access;};
static void *thread_walk(void *p) {
 struct thread *t=p;struct nested_ept_atomic_trace trace;
 t->result=walk(&t->fixture,t->access,&trace);return NULL;
}
static void real_collision(void) {
 for(unsigned round=0;round<50;round++){
  struct fixture f;init(&f);
  for(unsigned level=0;level<3;level++)pages[level][0]|=0x100;
  pthread_barrier_t barrier;assert(!pthread_barrier_init(&barrier,NULL,2));unsigned collisions=0;
  struct thread t[2]={0};pthread_t threads[2];
  for(unsigned i=0;i<2;i++){
   t[i].fixture=(struct fixture){.permissions=3,.barrier=&barrier,.collision_count=&collisions};
   t[i].access=((round&1)&&!i)?1:2;
   assert(!pthread_create(&threads[i],NULL,thread_walk,&t[i]));
  }
  for(unsigned i=0;i<2;i++){
   assert(!pthread_join(threads[i],NULL));
   assert(t[i].result.status==NEA_DONE&&t[i].result.translation.status==NESTED_EPT_OK);
  }
  assert(__atomic_load_n(&collisions,__ATOMIC_ACQUIRE)==1);
  unsigned changed=(unsigned)t[0].result.changed+(unsigned)t[1].result.changed;
  assert(changed>=1&&changed<=((round&1)?2u:1u));
  assert(__atomic_load_n(&pages[3][15],__ATOMIC_ACQUIRE)==0x9373);
  assert(!pthread_barrier_destroy(&barrier));
 }
 puts("PASS review 50 real two-thread guaranteed CAS conflicts, not merely final-bit observation");
}
int main(int argc,char **argv) {
 const char *only=argc>1?argv[1]:"all";
 #define RUN(n) do{if(!strcmp(only,"all")||!strcmp(only,#n))n();}while(0)
 RUN(explicit_memory);RUN(format_and_faults);RUN(partial_and_replay);RUN(real_collision);
 puts("PASS independent N41A atomic WB/IPAT1 walker review");
}
