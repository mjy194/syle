/* Actual retained fixture + catalog + replacement memory module. Only physical
 * allocation, host mapping and MTRR probes are mocked. No VM/hardware run. */
#define main legacy_memory_main
#include "nested41c_memory.c"
#undef main
#include "../../krnl/nested_source_replace_fixture.h"
#include <pthread.h>
static struct nested_source_replace_fixture f;
static const struct nrc_identity next_id={1,2,5,6};
static bool allow;
static unsigned guard_calls;
#ifdef NSRF_CAS_INJECT
static unsigned inject;
bool __real___atomic_compare_exchange_8(volatile void *,void *,uint64_t,int,int);
bool __wrap___atomic_compare_exchange_8(volatile void *ptr,void *expected,uint64_t desired,int success,int failure){
 bool target=ptr==&ram[18][15]&&desired==UINT64_C(0x1d073);
 if(target&&inject==1)__atomic_fetch_or(&ram[18][15],0x100,__ATOMIC_RELAXED);
 bool ok=__real___atomic_compare_exchange_8(ptr,expected,desired,success,failure);
 if(target&&inject==2)__atomic_fetch_or(&ram[26][15],0x100,__ATOMIC_RELAXED);
 if(target&&inject==3)allow=false;
 if(target&&inject>=4&&inject<=6){const unsigned pages[]={14,28,27};ram[pages[inject-4]][511]^=1;}
 if(target&&inject==7)denied_host=f.original.owned[28];
 if(target)inject=0;
 return ok;
}
#endif
static bool stopped(void *ctx,uint64_t ticket){assert(ctx==&f);guard_calls++;return allow&&ticket==71;}
static void setup(void){
 base(&d);memset(&f,0,sizeof f);allow=true;guard_calls=0;
 assert(nested_source_replace_fixture_prepare(&f,&d,id,next_id));
 assert(nested_atomic_memory_ready(&f.original)&&nested_source_replace_fixture_ready(&f));
 assert(!f.successor.initialized&&ram[28][0]==NSRF_NEW_SENTINEL);
 for(unsigned n=1;n<512;n++)assert(ram[28][n]==0);
 /* Vary every data word to detect whole-page corruption, not just sentinel. */
 const unsigned pages[]={14,28,27};
 for(unsigned p=0;p<3;p++)for(unsigned n=1;n<512;n++)ram[pages[p]][n]=UINT64_C(0xa170000000000000)|((uint64_t)p<<32)|n;
}
static void close_old(void){
 struct nrc_session s={0};uint64_t nonce=0;
 for(unsigned r=0;r<3;r++){
  assert(nrc_begin(&f.original.catalog,f.original.roots[r],&f.original,&s,&nonce));
  assert(nrc_end(&s,nonce));
 }
 assert(nonce==3&&nrc_close(&f.original.catalog,id)&&nrc_drained(&f.original.catalog,id));
}
static uint64_t translate(unsigned root){
 struct nrc_session s={0};uint64_t nonce;struct nrc_atomic_page a;
 assert(nrc_begin(&f.successor,f.roots[root],&f,&s,&nonce));
 assert(nonce>3);uint64_t g=f.roots[root].root_gpa;
 for(unsigned depth=4;depth;depth--){
  assert(nrc_resolve_atomic(&s,nonce,g,1,&a)&&a.atomic_view&&a.pinned_writable);
  unsigned i=(NSRF_TARGET_GPA>>(12+9*(depth-1)))&511;
  uint64_t e=__atomic_load_n(&a.atomic_view[i],__ATOMIC_ACQUIRE);
  assert(e&1);g=e&UINT64_C(0x0000fffffffff000);
 }
 assert(nrc_resolve_atomic(&s,nonce,g,3,&a)&&!a.atomic_view&&!a.pinned_writable&&a.backing.memory_type==6);
 uint64_t result=a.backing.hpa;assert(nrc_end(&s,nonce));return result;
}
static void positive(void){
 setup();
 /* Real live source A/D are present at the stopped cut and preserved exactly. */
 for(unsigned p=16;p<=27;p++)if((p>=16&&p<=20)||(p>=24&&p<=27))for(unsigned n=0;n<512;n++)
  if(ram[p-1][n])__atomic_fetch_or(&ram[p-1][n],p==19||p==27?0x300:0x100,__ATOMIC_ACQ_REL);
 close_old();static uint64_t before[NAM_OWNED_PAGES][512];memcpy(before,ram,sizeof ram);
 assert(nested_source_replace_fixture_change(&f,71,stopped,&f,64));
 assert(f.attempted&&f.changed&&f.published&&f.plan.started&&f.plan.applied&&f.plan.page_count==9);
 assert(f.successor.predecessor==&f.original.catalog&&f.successor.next_nonce==3);
 assert(f.original.catalog.superseded&&f.original.catalog.closed&&guard_calls);
 for(unsigned p=0;p<NAM_OWNED_PAGES;p++)for(unsigned n=0;n<512;n++)
  assert(ram[p][n]==(p==18&&n==15?UINT64_C(0x1d073):before[p][n]));
 assert(nested_source_replace_fixture_raw_unchanged(&f));
 assert(!nested_atomic_memory_ready(&f.original)); /* old immutable contract stays strict */
 assert(nested_source_replace_fixture_ready(&f));
 for(unsigned n=0;n<63;n++){
  assert(f.spans[n].object==f.original.spans[n].object);
  assert(f.spans[n].lifetime==f.original.spans[n].lifetime&&f.spans[n].lifetime==id.lifetime);
  assert(f.spans[n].hpa==f.original.spans[n].hpa);
 }
 for(unsigned r=0;r<3;r++){
  assert(f.roots[r].generation==2&&f.roots[r].lifetime==id.lifetime);
  assert(f.roots[r].full_eptp==f.original.roots[r].full_eptp);
  assert(translate(r)==d.backing.pages[r==2?27:28]);
 }
 struct nrc_session s={0};uint64_t nonce=0;
 assert(!nrc_begin(&f.original.catalog,f.original.roots[0],&f.original,&s,&nonce));
 assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,64));
 __atomic_fetch_or(&ram[18][15],0x300,__ATOMIC_ACQ_REL);
 assert(nested_source_replace_fixture_ready(&f)&&!nested_source_replace_fixture_raw_unchanged(&f));
 assert(translate(0)==d.backing.pages[28]);
 puts("PASS N47B retained two-bank real fixture, one-word CAS, independent successor and raw preservation");
}
static void negatives(void){
 for(unsigned bad=0;bad<17;bad++){
  setup();close_old();
  switch(bad){
  case 0:allow=false;break;
  case 1:denied_host=f.original.owned[28];break;
  case 2:denied_wb=f.original.owned[28];break;
  case 3:d.backing.pages[28]=d.backing.pages[27];break;
  case 4:ram[d.root01[0][3]/4096-1][29]^=0x40;break;
  case 5:ram[d.root01[1][3]/4096-1][16]|=2;break;
  case 6:ram[d.root02[0][3]/4096-1][80]=d.backing.pages[28]|0x73;break;
  case 7:ram[d.root02[1][2]/4096-1][1]=0xf7;break;
  case 8:ram[d.backing.tables[3]/4096-1][29]^=0x30;break;
  case 9:ram[18][16]|=2;break; /* TABLE leaf W */
  case 10:ram[26][16]|=4;break; /* TABLE leaf X */
  case 11:ram[18][15]=0x10071;break; /* old target is TABLE */
  case 12:ram[26][15]=0x1d073;break; /* C must remain unchanged */
  case 13:f.original.spans[28].lifetime++;break;
  case 14:f.original.roots[1].generation++;break;
  case 15:d.atomic_catalog_retained=false;break;
  case 16:f.original.catalog.references=1;break;
  }
  static uint64_t before[NAM_OWNED_PAGES][512];memcpy(before,ram,sizeof ram);
  assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,64));
  assert(!f.plan.started&&!f.successor.initialized&&!memcmp(before,ram,sizeof ram));
 }
 setup();assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,64)&&!f.attempted); /* open old */
 setup();close_old();assert(!nested_source_replace_fixture_change(&f,72,stopped,&f,64)&&!f.attempted);
 setup();close_old();assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,0)&&!f.attempted);
 setup();close_old();f.original.catalog.next_nonce=UINT64_MAX-31;
 assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,64)&&!f.attempted&&!f.plan.started);
 puts("PASS N47B exact authority, WB/IPAT, full maps, RO TABLE aliases, retained metadata and drain negatives");
}
/* Exercise nsrm's new graph-alias contract directly, without fixture-ready's
 * independent expected-graph rejection hiding a core validator regression. */
static bool core_authority(void *ctx,uint64_t ticket,const struct nrc_span *s,uint64_t g,struct nsrm_authority_page *out){
 (void)ctx;bool t=s->role==NRC_TABLE;
 *out=(struct nsrm_authority_page){.identity=id,.ticket=ticket,.gpa=g,.object=s->object,.lifetime=s->lifetime,
  .hpa=s->hpa,.atomic_view=t?(uint64_t *)s->table_view:NULL,.permissions=s->permissions,.memory_type=6,
  .ignore_pat=true,.pinned=true,.guest_aliases_ro=t,.updater_drained=true,.replacement_data_safe=!t&&g==NSRF_NEW_GPA};return true;
}
static void core_alias_negatives(void){
 for(unsigned bad=0;bad<6;bad++){
  setup();close_old();struct nsrm_request r={.ticket=71,.full_eptp=0x1005e,.l2_gpa=0xf000,
   .table_gpa=0x13000,.table_hpa=d.backing.pages[18],.table_object=19,.table_lifetime=id.lifetime,
   .expected_old=0xf073,.new_leaf=0x1d073,.new_gpa=0x1d000,.new_hpa=d.backing.pages[28],
   .new_object=29,.new_lifetime=id.lifetime,.index=15};
  if(bad==0)ram[18][16]|=2;
  if(bad==1)ram[26][16]|=4;
  if(bad==2){r.expected_old=ram[18][15]=0x10071;}
  if(bad==3){r.new_leaf=0x10071;r.new_gpa=0x10000;r.new_hpa=d.backing.pages[15];r.new_object=16;}
  if(bad==4){f.original.spans[15].permissions=3;ram[18][16]|=2;}
  if(bad==5)f.original.spans[15].permissions=3; /* catalog TABLE itself must remain R */
  static struct nsrm_plan plan;memset(&plan,0,sizeof plan);
  assert(!nsrm_prepare(&plan,&f.original.catalog,core_authority,&f,r)&&!plan.started);
 }
 puts("PASS N47B nsrm direct TABLE W/X alias and old/new TABLE destination rejection");
}
static unsigned go;
static void *concurrent_owner(void *arg){
 unsigned owner=*(unsigned *)arg;
 while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))__asm__ volatile("pause");
 for(unsigned iteration=0;iteration<120;iteration++){
  struct nrc_session s={0};uint64_t nonce=0;struct nrc_atomic_page a;
  unsigned root=(owner+iteration)%3;
  assert(nrc_begin(&f.successor,f.roots[root],&f,&s,&nonce));
  assert(nested_source_replace_fixture_ready(&f));
  uint64_t g=f.roots[root].root_gpa;
  for(unsigned depth=4;depth;depth--){
   assert(nrc_resolve_atomic(&s,nonce,g,1,&a)&&a.atomic_view);
   unsigned index=(NSRF_TARGET_GPA>>(12+9*(depth-1)))&511;
   uint64_t e=__atomic_fetch_or(&a.atomic_view[index],depth==1?0x300:0x100,__ATOMIC_ACQ_REL);
   g=e&UINT64_C(0x0000fffffffff000);
   assert(nested_source_replace_fixture_ready(&f));
  }
  assert(nrc_resolve_atomic(&s,nonce,g,3,&a)&&!a.atomic_view&&
    a.backing.hpa==d.backing.pages[root==2?27:28]);
  assert(nrc_end(&s,nonce));
 }
 return NULL;
}
static void concurrent_successor(void){
 setup();close_old();assert(nested_source_replace_fixture_change(&f,71,stopped,&f,512));
 pthread_t threads[2];unsigned owners[]={0,1};go=0;
 for(unsigned n=0;n<2;n++)assert(!pthread_create(&threads[n],NULL,concurrent_owner,&owners[n]));
 __atomic_store_n(&go,1,__ATOMIC_RELEASE);
 for(unsigned n=0;n<2;n++)assert(!pthread_join(threads[n],NULL));
 assert(nrc_idle(&f.successor,next_id)&&nested_source_replace_fixture_ready(&f));
 assert(f.successor.next_nonce==243); /* joined: safe exclusive observation */
 puts("PASS N47B two concurrent successor owners begin/resolve/end/ready and legal atomic source A/D");
}
#ifdef NSRF_CAS_INJECT
static void injected_failures(void){
 for(unsigned bad=1;bad<=7;bad++){
  setup();close_old();inject=bad;
  assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,64));
  assert(f.plan.started&&!f.published&&!f.successor.initialized&&d.atomic_catalog_retained);
  assert(ram[18][15]==(bad==1?UINT64_C(0xf173):UINT64_C(0x1d073)));
  assert(!nested_source_replace_fixture_change(&f,71,stopped,&f,64));
 }
 puts("PASS N47B injected CAS loss, post-CAS source/data/host/guard failure, no retry or rollback");
}
#endif
int main(void){positive();negatives();core_alias_negatives();concurrent_successor();
#ifdef NSRF_CAS_INJECT
 injected_failures();
#endif
 return 0;
}
