#include "../ad_updater_gate.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
static struct ad_entry_stamp stamp={1,2,3,4,5};
static struct ad_updater_gate gate;
static unsigned ready,closed,finish;
static void *worker(void *unused){
 uint64_t nonce=0,extra_nonce=0;(void)unused;struct ad_updater_token t={0},extra={0};
 assert(ad_updater_gate_acquire(&gate,stamp,&t,&nonce));
 __atomic_fetch_add(&ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&closed,__ATOMIC_ACQUIRE))sched_yield();
 assert(ad_updater_gate_guard(&gate,&t,stamp,nonce));
 assert(!ad_updater_gate_acquire(&gate,stamp,&extra,&extra_nonce));
 while(!__atomic_load_n(&finish,__ATOMIC_ACQUIRE))sched_yield();
 assert(ad_updater_gate_release(&gate,&t,stamp,nonce));return 0;
}
int main(void){
 uint64_t nonce=0;struct ad_updater_gate g;struct ad_updater_token t={0};struct ad_entry_stamp next={0};
 assert(ad_updater_gate_init(&g,stamp));
 assert(ad_updater_gate_idle(&g,stamp));
 assert(ad_updater_gate_acquire(&g,stamp,&t,&nonce));
 assert(!ad_updater_gate_idle(&g,stamp));
 assert(!ad_updater_gate_acquire(&g,stamp,&t,&nonce));
 struct ad_updater_token copy=t;
 assert(!ad_updater_gate_release(&g,&copy,stamp,nonce));
 assert(!ad_updater_gate_guard(&g,&copy,stamp,nonce));
 for(unsigned field=0;field<5;field++){
  struct ad_entry_stamp bad=stamp;
  switch(field){case 0:bad.domain_id++;break;case 1:bad.incarnation++;break;
   case 2:bad.context_id++;break;case 3:bad.epoch++;break;case 4:bad.lifetime++;break;}
  assert(!ad_updater_gate_guard(&g,&t,bad,nonce));assert(!ad_updater_gate_release(&g,&t,bad,nonce));
  assert(!ad_updater_gate_close(&g,bad));
 }
 assert(ad_updater_gate_close(&g,stamp));assert(!ad_updater_gate_close(&g,stamp));
 assert(!ad_updater_gate_drained(&g,stamp));assert(ad_updater_gate_guard(&g,&t,stamp,nonce));
 assert(!ad_updater_gate_reopen(&g,stamp,true,&next));
 assert(ad_updater_gate_release(&g,&t,stamp,nonce));assert(!ad_updater_gate_release(&g,&t,stamp,nonce));
 assert(ad_updater_gate_drained(&g,stamp));assert(!ad_updater_gate_idle(&g,stamp));
 assert(!ad_updater_gate_reopen(&g,stamp,false,&next)&&next.epoch==0);
 assert(ad_updater_gate_reopen(&g,stamp,true,&next)&&next.epoch==5);
 assert(!ad_updater_gate_idle(&g,stamp));assert(ad_updater_gate_idle(&g,next));
 assert(!ad_updater_gate_acquire(&g,stamp,&t,&nonce));
 assert(ad_updater_gate_acquire(&g,next,&t,&nonce));
 uint64_t stale_nonce=nonce;assert(ad_updater_gate_release(&g,&t,next,nonce));
 assert(ad_updater_gate_acquire(&g,next,&t,&nonce)&&nonce!=stale_nonce);
 assert(!ad_updater_gate_release(&g,&t,next,stale_nonce));
 assert(!ad_updater_gate_guard(&g,&t,next,stale_nonce));ad_updater_gate_poison(&g);
 assert(!ad_updater_gate_guard(&g,&t,next,nonce));assert(ad_updater_gate_release(&g,&t,next,nonce));
 assert(!ad_updater_gate_acquire(&g,next,&t,&nonce));
 struct ad_entry_stamp exhausted=stamp;exhausted.epoch=UINT64_MAX;
 assert(ad_updater_gate_init(&g,exhausted));assert(ad_updater_gate_close(&g,exhausted));
 assert(!ad_updater_gate_reopen(&g,exhausted,true,&next));
 assert(ad_updater_gate_init(&g,stamp));g.next_nonce=UINT64_MAX;
 assert(!ad_updater_gate_acquire(&g,stamp,&t,&nonce)&&g.references==0);
 assert(ad_updater_gate_init(&g,stamp));g.lock=1;
 assert(!ad_updater_gate_acquire(&g,stamp,&t,&nonce)&&g.fault&&g.lock==1);
 g.lock=0;assert(!ad_updater_gate_drained(&g,stamp));
 assert(ad_updater_gate_init(&gate,stamp));pthread_t workers[8];
 for(unsigned i=0;i<8;i++)assert(!pthread_create(&workers[i],0,worker,0));
 while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=8)sched_yield();
 assert(ad_updater_gate_close(&gate,stamp));__atomic_store_n(&closed,1,__ATOMIC_RELEASE);
 assert(!ad_updater_gate_drained(&gate,stamp));
 __atomic_store_n(&finish,1,__ATOMIC_RELEASE);
 for(unsigned i=0;i<8;i++)assert(!pthread_join(workers[i],0));
 assert(ad_updater_gate_drained(&gate,stamp));assert(ad_updater_gate_reopen(&gate,stamp,true,&next));
 puts("ad_updater_gate: PASS (8 concurrent whole-call leases, close/drain)");
}
