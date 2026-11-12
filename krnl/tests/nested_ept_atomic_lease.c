#include "../nested_ept_atomic_lease.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t pages[4][512];
static struct ad_updater_gate gate;
static struct ad_entry_stamp stamp={1,2,3,4,5};
static unsigned arrived,proceed;
struct worker { bool pause; uint8_t need; unsigned calls; struct nested_ept_atomic_session session; struct nested_ept_atomic_result result; };
static bool guard(void *p,struct ad_entry_stamp s) {
 (void)p;return s.domain_id==1&&s.incarnation==2&&s.context_id==3&&s.lifetime==5;
}
static bool table(void *p,uint64_t g,uint64_t*h,uint64_t**v) {
 struct worker*w=p;
 if(w->pause&&!w->calls++) {
  __atomic_fetch_add(&arrived,1,__ATOMIC_RELEASE);
  while(!__atomic_load_n(&proceed,__ATOMIC_ACQUIRE))sched_yield();
 }
 if(g<0x1000||g>0x4000||(g&4095))return false;
 *h=g;*v=pages[g/4096-1];return true;
}
static bool leaf(void*p,uint64_t g,uint64_t*h,uint8_t*perm) {
 (void)p;if(g!=0x8000)return false;*h=0x18000;*perm=3;return true;
}
static const struct nested_ept_atomic_ops ops={guard,table,leaf,0,0};
static void *run(void*p) {
 struct worker*w=p;struct nested_ept_atomic_trace trace;
 w->result=nested_ept_atomic_walk_leased(&gate,&w->session,0x105e,0x8123,w->need,48,&ops,w,stamp,&trace);
 return 0;
}
static uint64_t atomic_load(void*p,uint64_t*v) {
 (void)p;return __atomic_load_n(v,__ATOMIC_ACQUIRE);
}
static bool stuck_after_cas(void*p,uint64_t*v,uint64_t old,uint64_t value) {
 (void)p;bool ok=__atomic_compare_exchange_n(v,&old,value,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 if(ok)__atomic_store_n(&gate.lock,1,__ATOMIC_RELEASE); /* injected suspended lock owner */
 return ok;
}
int main(void) {
 memset(pages,0,sizeof pages);pages[0][0]=0x2007;pages[1][0]=0x3007;pages[2][0]=0x4007;pages[3][8]=0x8037;
 assert(ad_updater_gate_init(&gate,stamp));
 struct worker a={.pause=true,.need=1},b={.pause=true,.need=2};pthread_t ta,tb;
 assert(!pthread_create(&ta,0,run,&a));assert(!pthread_create(&tb,0,run,&b));
 while(__atomic_load_n(&arrived,__ATOMIC_ACQUIRE)!=2)sched_yield();
 assert(ad_updater_gate_close(&gate,stamp));assert(!ad_updater_gate_drained(&gate,stamp));
 struct worker denied={.need=1};run(&denied);assert(denied.result.status==NEA_STALE&&!denied.session.acquired&&!denied.calls);
 struct ad_entry_stamp next;
 assert(!ad_updater_gate_reopen(&gate,stamp,true,&next));
 __atomic_store_n(&proceed,1,__ATOMIC_RELEASE);
 assert(!pthread_join(ta,0));assert(!pthread_join(tb,0));
 assert(a.result.status==NEA_DONE&&b.result.status==NEA_DONE);
 assert(a.session.released&&b.session.released);
 assert(a.result.translation.hpa==0x18123&&b.result.translation.hpa==0x18123);
 assert((__atomic_load_n(&pages[3][8],__ATOMIC_ACQUIRE)&0x300)==0x300);
 for(unsigned i=0;i<3;i++)assert(__atomic_load_n(&pages[i][0],__ATOMIC_ACQUIRE)&0x100);
 assert(ad_updater_gate_drained(&gate,stamp));
 /* Pure caller attests both consumers stopped and all 02/trace invalidations
  * complete. This test has no EPT consumers and does not execute INVEPT. */
 for(unsigned i=0;i<3;i++)__atomic_fetch_and(&pages[i][0],~UINT64_C(0x300),__ATOMIC_ACQ_REL);
 __atomic_fetch_and(&pages[3][8],~UINT64_C(0x300),__ATOMIC_ACQ_REL);
 assert(!ad_updater_gate_reopen(&gate,stamp,false,&next));
 assert(ad_updater_gate_reopen(&gate,stamp,true,&next));
 struct worker stale={.need=1};run(&stale);assert(stale.result.status==NEA_STALE);
 stamp=next;struct worker reader={.need=1};run(&reader);
 assert(reader.result.status==NEA_DONE&&reader.session.released);
 assert((__atomic_load_n(&pages[3][8],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert(!ad_updater_gate_release(&gate,&a.session.token,stamp,a.session.nonce));
 assert(ad_updater_gate_close(&gate,stamp)&&ad_updater_gate_drained(&gate,stamp));
 /* Release failure cannot leave a dangling stack token: session is owned by
  * the caller until cleanup succeeds. Simulate an interrupted metadata owner. */
 assert(ad_updater_gate_reopen(&gate,stamp,true,&next));stamp=next;
 __atomic_fetch_and(&pages[0][0],~UINT64_C(0x100),__ATOMIC_ACQ_REL);
 struct worker retained={.need=1};struct nested_ept_atomic_trace tr;
 const struct nested_ept_atomic_ops injected={guard,table,leaf,atomic_load,stuck_after_cas};
 retained.result=nested_ept_atomic_walk_leased(&gate,&retained.session,0x105e,0x8000,1,48,&injected,&retained,stamp,&tr);
 assert(retained.result.status==NEA_STALE&&retained.result.changed);
 assert(retained.session.acquired&&!retained.session.released);
 __atomic_store_n(&gate.lock,0,__ATOMIC_RELEASE); /* simulated owner finally releases */
 assert(ad_updater_gate_release(&gate,&retained.session.token,stamp,retained.session.nonce));
 assert(!ad_updater_gate_acquire(&gate,stamp,&retained.session.token,&retained.session.nonce));
 puts("PASS N35A actual atomic dual-walker lease/closed drain/new epoch/A-only retrack (pure, no VMX)");
}
