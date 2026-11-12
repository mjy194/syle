/* Real runtime types and the entire production checkpoint body. External
 * VMX/round/memory helpers are controlled fixtures, not hardware claims. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "nested_parallel.h"
#include "nested_source_replace_fixture.h"

static struct nested_parallel_runtime run;
static struct nested_source_replace_round source_round;
static struct {
 struct nested_source_replace_fixture memory;
 struct ad_updater_gate gate[2];
 struct ad_entry_stamp stamp;
 uint64_t host_root[2],host_pat[2];
 uint32_t arrived[8][2],released[8];
} atomic_runtime;
static struct nested_ept_context_item active;
static struct tr_registry_slot active_slot;
static unsigned current_phase,failed,logged,checked;
static uint64_t current_vmcs,target_leaf;
static bool healthy,before_ok,current_ok,stopped_ok,slot_ok,leaf_ok;

static bool atomic_failed(void){return !healthy;}
static void atomic_fail(void){failed++;healthy=false;}
bool nsrr_before_entry(struct nested_source_replace_round *r,unsigned c){assert(r==&source_round);return c<2&&before_ok;}
static struct tr_registry_slot *atomic_active_slot(struct nested_owner_atomic *a){assert(a==&run.atomic_catalog);return slot_ok?&active_slot:NULL;}
static bool source_current(void *r,uint64_t *v){assert(r==&run);*v=current_vmcs;return current_ok;}
bool nested_owner_atomic_stopped(void *a){assert(a==&run.atomic_catalog);return stopped_ok;}
bool nested_owner_atomic_leaf(struct nested_owner_atomic *a,uint64_t g,uint64_t *v){assert(a==&run.atomic_catalog&&g==0xf000);*v=target_leaf;return leaf_ok;}
static bool source_host_audit(unsigned c){assert(c==run.cpu_index);return true;}
bool nested_parallel_trace_text(struct nested_parallel_runtime *r,const char *s){assert(r==&run);if(!strcmp(s,"N47B_CHECKPOINT="))logged++;return true;}
bool nested_parallel_trace_hex(struct nested_parallel_runtime *r,uint64_t v){assert(r==&run);(void)v;return true;}
static bool atomic_wait(uint32_t *p){(void)p;return true;}
bool nsrr_run(struct nested_source_replace_round *r,unsigned c){assert(r==&source_round&&c==run.cpu_index);return true;}
static uint64_t source_leaf(bool control){return (control?0x1c073u:0x1d073u)|((current_phase<3||current_phase>=5||control)?0x300u:0x100u);}
static bool source_round_evidence(struct nested_parallel_runtime *r){assert(r==&run);return true;}
bool nested_source_replace_fixture_ready(const struct nested_source_replace_fixture *m){assert(m==&atomic_runtime.memory);return true;}
static bool source_data(unsigned phase){assert(phase==current_phase);return true;}
bool ad_updater_gate_idle(struct ad_updater_gate *g,struct ad_entry_stamp s){assert(g==&atomic_runtime.gate[current_phase>=2]);(void)s;return true;}

#ifdef CHECKPOINT_MUTANT
#include "mutant-checkpoint.inc"
#else
#include "selected-checkpoint.inc"
#endif

static const unsigned resident[8]={1,2,2,1,2,2,2,2};
static const unsigned creations[2][8]={{1,2,2,3,4,4,4,4},{1,2,3,4,5,5,6,7}};
static const uint64_t selected[2][8]={
 {0x1005e,0x1405e,0x1005e,0x1005e,0x1405e,0x1005e,0x1005e,0x1405e},
 {0x1005e,0x1405e,0x1805e,0x1805e,0x1405e,0x1405e,0x1005e,0x1805e}};
static const unsigned promotions[2][8][3]={
 {{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{2,1,0},{2,1,0},{2,1,0}},
 {{1,0,0},{1,1,0},{1,1,1},{1,1,1},{1,1,1},{1,1,1},{2,1,1},{2,1,1}}};
static const bool writable[2][8]={{true,true,true,false,false,true,true,false},
                                 {true,true,true,false,false,false,true,false}};
static void setup(unsigned cpu,unsigned phase){
 assert(cpu<2&&phase<8);
 memset(&run,0,sizeof run);memset(&atomic_runtime,0,sizeof atomic_runtime);
 memset(&source_round,0,sizeof source_round);memset(&active,0,sizeof active);
 current_phase=phase;failed=logged=0;healthy=before_ok=current_ok=stopped_ok=slot_ok=leaf_ok=true;
 run.cpu_index=cpu;run.vcpu.vmcs01=current_vmcs=0x100000;
 run.sparse.selected12=selected[cpu][phase];
 run.sparse.hardware_item=run.sparse.contexts.active=&active;
 run.sparse.contexts.count=resident[phase];run.sparse.creations=creations[cpu][phase];
 for(unsigned k=0;k<3;k++)run.atomic_catalog.target_promotions[k]=promotions[cpu][phase][k];
 active.tree.eptp=0x20005e;target_leaf=0x300171|(writable[cpu][phase]?0x202:0);
 atomic_runtime.memory.original.catalog.references=4;
}
static void rejected(bool latched){
 assert(!atomic_checkpoint(&run,current_phase));assert(failed==(unsigned)latched);
 assert(!logged&&!atomic_runtime.arrived[current_phase][run.cpu_index]);checked++;
}
int main(void){
#ifdef EXPECT_LEGACY_COUNT
 for(unsigned cpu=0;cpu<2;cpu++){
  setup(cpu,3);rejected(true);
  setup(cpu,3);run.sparse.contexts.count=2;assert(atomic_checkpoint(&run,3));
 }
 puts("REPRODUCED exact v3: phase3 count1 rejected; incorrect count2 accepted for both owners");
#else
 /* First positive is the raw R6 BSP state: count1, cumulative creations3. */
 setup(0,3);assert(atomic_checkpoint(&run,3));assert(!failed&&logged==1);
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned phase=0;phase<8;phase++){
  for(unsigned count=0;count<4;count++){
   setup(cpu,phase);run.sparse.contexts.count=count;
   if(count==resident[phase]){
    assert(atomic_checkpoint(&run,phase));assert(!failed&&logged==1);
    assert(atomic_runtime.arrived[phase][cpu]==1);
    assert(cpu||atomic_runtime.released[phase]==1);checked++;
   }else rejected(true);
  }
  setup(cpu,phase);run.sparse.creations--;rejected(true);
  setup(cpu,phase);run.sparse.creations++;rejected(true);
  setup(cpu,phase);run.sparse.creations=UINT64_MAX;rejected(true);
 }
 for(unsigned cpu=0;cpu<2;cpu++){
  setup(cpu,3);assert(!atomic_checkpoint(&run,8));assert(!failed&&!logged);checked++;
  setup(cpu,3);assert(!atomic_checkpoint(&run,UINT_MAX));assert(!failed&&!logged);checked++;
  setup(cpu,3);assert(!atomic_checkpoint(NULL,3));assert(!failed&&!logged);checked++;
  setup(cpu,3);healthy=false;rejected(false);
  setup(cpu,3);before_ok=false;rejected(false);
  setup(cpu,3);current_ok=false;rejected(true);
  setup(cpu,3);current_vmcs++;rejected(true);
  setup(cpu,3);stopped_ok=false;rejected(true);
  setup(cpu,3);slot_ok=false;rejected(true);
  setup(cpu,3);run.sparse.selected12^=0x4000;rejected(true);
  setup(cpu,3);run.sparse.hardware_item=NULL;rejected(true);
  setup(cpu,3);leaf_ok=false;rejected(true);
  setup(cpu,3);target_leaf^=0x40;rejected(true);
  setup(cpu,3);target_leaf^=0x100;rejected(true);
  setup(cpu,3);run.atomic_catalog.atomic.fault.active=true;rejected(true);
  setup(cpu,3);run.atomic_catalog.atomic.entry_active=true;rejected(true);
  setup(cpu,3);target_leaf|=2;rejected(true);
  setup(cpu,3);target_leaf|=0x200;rejected(true);
  for(unsigned k=0;k<3;k++){setup(cpu,3);run.atomic_catalog.target_promotions[k]++;rejected(true);}
 }
 printf("PASS complete production checkpoint: R6 phase3 positive plus %u boundary/guard cases; real types, controlled external helpers; no VMX claim\n",checked);
#endif
 return 0;
}
