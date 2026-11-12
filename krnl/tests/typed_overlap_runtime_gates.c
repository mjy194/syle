/* Compiles unchanged production entry and terminal-checkpoint functions.
 * Stub only the physical guard and stopped-domain actions; no VMX executes. */
#include "../nested_parallel.h"
#include "../typed_root_overlap.h"
#include "../cache_mp_transition.h"
#include "../cache_maintenance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {
 struct typed_root_registry bridges[2];struct typed_directory directory;
 struct typed_root_lease lease;struct trl_binding fixed_bindings[5];
 struct cache_mp_transition transition;struct cache_cpu_snapshot snapshots[2];
 struct tr_overlap overlap[2];struct tr_overlap_counts overlap_before[2];
 bool overlap_entered[2],initialized,source_registered;
 uint64_t source_epoch,ack_sequence,sequence,command_epoch;
 unsigned command;uint32_t turn,teardown_turn,stopped[4][2],released[4];
} typed_runtime;
static struct {struct {uint64_t pages[63];} backing;} domain_memory;
static unsigned guard_calls,wait_calls,release_after;
static bool fault,checkpoint_mode;
static uint64_t time_now;
static bool typed_failed(void){return fault;}
static void typed_fatal(void *r){(void)r;fault=true;}
static uint64_t typed_clock(void){
 if(checkpoint_mode)typed_runtime.released[3]=1;
 time_now+=UINT64_C(30000000001);return time_now;
}
static uint64_t wait_now(void *unused){(void)unused;return time_now++;}
static void wait_relax(void *unused){(void)unused;if(release_after&&!--release_after)typed_runtime.teardown_turn=1;}
static bool wait_healthy(void *unused){(void)unused;return !fault;}
static bool wait_owned(void *unused){(void)unused;return false;}
static bool typed_overlap_wait(uint32_t *word){
 wait_calls++;const struct tr_overlap_wait w={wait_now,wait_relax,wait_healthy,wait_owned,NULL,5};
 return tr_overlap_wait_for(word,&w);
}
static bool typed_guard(void *r){(void)r;guard_calls++;return false;}
static bool typed_meta_lock(void){return true;}
static void typed_meta_unlock(void){}
static bool typed_overlap_checkpoint(struct nested_parallel_runtime *r,unsigned phase){(void)r;return phase==3;}
static bool fake_precheck(uint64_t h,struct cache_cpu_snapshot *s){(void)h;(void)s;return true;}
#define cache_maintenance_precheck fake_precheck
static bool typed_local(struct nested_parallel_runtime *r,unsigned c,uint64_t e){(void)r;(void)c;(void)e;return true;}
static bool typed_initialize(void){return true;}
static bool typed_terminal(void){return true;}
static bool typed_transition(unsigned phase){(void)phase;return true;}
static bool typed_wait32(uint32_t *word){return *word!=0;}
bool nested_parallel_trace_text(struct nested_parallel_runtime *r,const char *s){(void)r;(void)s;return true;}
bool nested_parallel_trace_hex(struct nested_parallel_runtime *r,uint64_t n){(void)r;(void)n;return true;}
#include "typed-overlap-runtime-gates.inc"
static void configure(void){memset(&typed_runtime,0,sizeof typed_runtime);typed_runtime.initialized=true;guard_calls=wait_calls=release_after=0;fault=checkpoint_mode=false;time_now=0;}
int main(void){
 struct nested_parallel_runtime r={0};r.cpu_index=1;
 configure();typed_runtime.overlap[0].held=1;
 typed_runtime.overlap[0].hold=(struct tr_overlap_hold){.phase=0,.cpu=0,.completed.scope.ticket.type_epoch=1};
 /* Legacy turn remains zero. B2 must nevertheless let this peer progress
  * once the held record is acquired; the guard deliberately ends the test. */
 assert(!typed_context_before_entry(&r,false));assert(guard_calls==1&&wait_calls==1);
 configure();assert(!typed_context_before_entry(&r,false));assert(!guard_calls&&fault);
 configure();checkpoint_mode=true;typed_runtime.released[1]=1;release_after=2;
 assert(typed_checkpoint(&r,3));assert(wait_calls==1&&typed_runtime.teardown_turn==1&&!fault);
 configure();checkpoint_mode=true;typed_runtime.released[1]=1;
 assert(!typed_checkpoint(&r,3));assert(wait_calls==1&&fault&&!typed_runtime.teardown_turn);
 puts("PASS N39B2 unchanged runtime gates: peer proceeds despite round turn, missing hold times out, terminal phase3 waits before returning");
}
