#ifndef SYLE_TEST_OVERLAP_FIXTURE_H
#define SYLE_TEST_OVERLAP_FIXTURE_H
#include "../typed_root_overlap.h"
#include <assert.h>
static inline struct tr_local_completed overlap_completed(unsigned cpu,unsigned phase){
 struct td_retire_ticket ticket={.identity={10,11,100+cpu,1,200+cpu,cpu},
  .full_eptp=0x10001e + cpu*0x100000,.hpa=0x100000+cpu*0x100000,
  .cache_generation=1,.history=UINT64_C(1)<<cpu,.type_epoch=phase+1,.nonce=30+cpu};
 return (struct tr_local_completed){.scope={ticket,40+cpu},
  .invalidation={ticket.full_eptp,ticket.history,cpu,true},.acked=ticket.history,.flush_serial=50+cpu,.complete=true};
}
static inline struct tr_overlap_receipt overlap_receipt(unsigned cpu,unsigned phase,uint64_t before,uint64_t commit){
 static struct typed_directory directory;
 struct tr_local_completed c=overlap_completed(cpu,phase);struct td_retire_ticket t=c.scope.ticket;
 return (struct tr_overlap_receipt){.completed=c,.evidence_version=before,
  .receipt={.directory=&directory,.identity=t.identity,.full_eptp=t.full_eptp,.hpa=t.hpa,
   .cache_generation=t.cache_generation,.history=t.history,.acked=t.history,.flush_serial=c.flush_serial,
   .type_epoch=t.type_epoch,.retirement_nonce=t.nonce,.commit_version=commit,.live=true}};
}
static inline struct tr_overlap overlap_fixture(unsigned phase){
 struct tr_overlap o={0};
 struct tr_overlap_hold h={overlap_completed(phase,phase),10,phase,phase};
 assert(tr_overlap_publish_hold(&o,h));
 struct tr_overlap_peer p={.retired=overlap_receipt(phase^1,phase,15,20),
  .before={1,1,5,2,6},.after={3,2,10,3,9},.phase=phase,.cpu=phase^1,
  .switched={.sequence=6,.completed=2}};
 for(unsigned step=1;step<=3;step++)assert(tr_overlap_switch_step(&p.switched,step,0x30001e,0x40001e));
 assert(tr_overlap_publish_peer(&o,p));
 assert(tr_overlap_finish(&o,overlap_receipt(phase,phase,9,25)));
 return o;
}
#endif
