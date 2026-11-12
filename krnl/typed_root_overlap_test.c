/* Production causal-publication protocol. No hardware instruction is mocked
 * as hardware proof; actual VMX execution is a separate acceptance stage. */
#include "tests/typed_root_overlap_fixture.h"
#include <stdio.h>
#include <string.h>
static void mutations(void){
 for(unsigned phase=0;phase<2;phase++){
  struct tr_overlap good=overlap_fixture(phase);assert(tr_overlap_valid(&good));
  assert(tr_overlap_receipt_equal(good.final,good.final));
  for(unsigned m=0;m<43;m++){
   struct tr_overlap o=good;
   switch(m){
   case 0:o.held=0;break;
   case 1:o.progress=0;break;
   case 2:o.finished=0;break;
   case 3:o.hold.phase^=1;break;
   case 4:o.hold.cpu^=1;break;
   case 5:o.hold.version=0;break;
   case 6:o.hold.version=o.peer.retired.receipt.commit_version;break;
   case 7:o.hold.completed.complete=false;break;
   case 8:o.hold.completed.invalidation.success=false;break;
   case 9:o.hold.completed.invalidation.cpu^=1;break;
   case 10:o.hold.completed.invalidation.full_eptp^=4096;break;
   case 11:o.hold.completed.invalidation.acked^=3;break;
   case 12:o.hold.completed.scope.operation_nonce++;break;
   case 13:o.hold.completed.scope.ticket.nonce++;break;
   case 14:o.hold.completed.scope.ticket.type_epoch++;break;
   case 15:o.hold.completed.flush_serial++;break;
   case 16:o.peer.phase^=1;break;
   case 17:o.peer.cpu^=1;break;
   case 18:o.peer.retired.completed.scope.ticket.identity.domain++;break;
   case 19:o.peer.retired.receipt.acked^=3;break;
   case 20:o.peer.retired.receipt.retirement_nonce++;break;
   case 21:o.peer.retired.receipt.type_epoch++;break;
   case 22:o.peer.retired.receipt.flush_serial++;break;
   case 23:o.peer.retired.evidence_version=o.hold.version-1;break;
   case 24:o.peer.retired.receipt.commit_version=o.hold.version;break;
   case 25:o.peer.after.creations=o.peer.before.creations;break;
   case 26:o.peer.after.evictions=o.peer.before.evictions;break;
   case 27:o.peer.after.misses=o.peer.before.misses;break;
   case 28:o.peer.after.switches=o.peer.before.switches;break;
   case 29:o.peer.switched.stage=1;break;
   case 30:o.peer.switched.old_eptp=o.peer.switched.new_eptp;break;
   case 31:o.peer.switched.write=o.peer.switched.pin;break;
   case 32:o.peer.switched.unpin=o.peer.switched.write;break;
   case 33:o.peer.before.switch_sequence=o.peer.switched.pin;break;
   case 34:o.final.receipt.commit_version=o.peer.retired.receipt.commit_version;break;
   case 35:o.final.completed.scope.operation_nonce++;break;
   case 36:o.final.completed.scope.ticket.identity.allocator_lifetime++;break;
   case 37:o.final.evidence_version=o.hold.version+1;break;
   case 38:o.final.receipt.live=false;break;
   case 39:o.final.receipt.directory=NULL;break;
   case 40:o.final.completed.acked^=3;break;
   case 41:o.final.completed.scope.ticket.hpa+=4096;break;
   case 42:o.final.completed.scope.ticket.cache_generation++;break;
   }
   assert(!tr_overlap_valid(&o));
  }
  struct tr_overlap empty={0};
  assert(!tr_overlap_publish_peer(&empty,good.peer));assert(!tr_overlap_finish(&empty,good.final));
  assert(tr_overlap_publish_hold(&empty,good.hold));assert(!tr_overlap_publish_hold(&empty,good.hold));
  assert(!tr_overlap_finish(&empty,good.final));assert(tr_overlap_publish_peer(&empty,good.peer));
  assert(!tr_overlap_publish_peer(&empty,good.peer));assert(tr_overlap_finish(&empty,good.final));
  assert(!tr_overlap_finish(&empty,good.final));
  struct tr_overlap_receipt other=good.final;other.completed.scope.operation_nonce++;
  assert(!tr_overlap_receipt_equal(other,good.final));
 }
}
struct clock_test {uint64_t time;uint32_t word;unsigned cycles;bool fault,own;};
static uint64_t now(void *p){return ((struct clock_test *)p)->time++;}
static bool healthy(void *p){return !((struct clock_test *)p)->fault;}
static bool owned(void *p){return ((struct clock_test *)p)->own;}
static void relax(void *p){struct clock_test *c=p;if(c->cycles&&!--c->cycles)__atomic_store_n(&c->word,1,__ATOMIC_RELEASE);}
static void waits(void){
 struct clock_test c={0};struct tr_overlap_wait w={now,relax,healthy,owned,&c,8};
 assert(!tr_overlap_wait_for(&c.word,&w));assert(c.time==9);
 c=(struct clock_test){.cycles=2};assert(tr_overlap_wait_for(&c.word,&w));
 c=(struct clock_test){.word=1,.fault=true};assert(!tr_overlap_wait_for(&c.word,&w));
 c=(struct clock_test){.word=1,.own=true};assert(!tr_overlap_wait_for(&c.word,&w));
 c=(struct clock_test){.word=1};assert(tr_overlap_wait_for(&c.word,&w));
}
static void switches(void){
 struct tr_overlap_switch s={0};
 assert(!tr_overlap_switch_step(&s,2,0x10001e,0));
 assert(tr_overlap_switch_step(&s,1,0x10001e,0));
 assert(!tr_overlap_switch_step(&s,3,0x10001e,0));
 assert(!tr_overlap_switch_step(&s,2,0x20001e,0));
 assert(tr_overlap_switch_step(&s,2,0x10001e,0));
 assert(tr_overlap_switch_step(&s,3,0x10001e,0));assert(!s.completed);
 for(unsigned step=1;step<=3;step++)assert(tr_overlap_switch_step(&s,step,0x10001e,0x10001e));
 assert(!s.completed);
 for(unsigned step=1;step<=3;step++)assert(tr_overlap_switch_step(&s,step,0x20001e,0x10001e));
 assert(s.completed==1&&s.pin<s.write&&s.write<s.unpin);
 s.sequence=UINT64_MAX;assert(!tr_overlap_switch_step(&s,1,0x10001e,0));
}
int main(void){mutations();waits();switches();puts("PASS N39B2 production overlap protocol: scoped WB/UC causal receipts, switch order, 86 corruptions, duplicates, timeout, own-lock refusal");}
