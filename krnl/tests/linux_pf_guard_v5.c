#include "../linux_pf_guard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 struct lpfg_state s={0},n,unchanged;
 struct lve_pf_exit e={.reason=0,.qualification=0x1000000,.rip=0x19737f5,.interruption_info=0x80000b0e,.interruption_error=2,.idt_base=0x1979050,.idt_limit=0x1ff};
 struct lve_pf_entry inj;
 for(unsigned i=0;i<4;i++){
  e.qualification=0x1000000ULL+(uint64_t)i*0x200000;
  assert(lpfg_inject(&s,&e,0x1991000,0,&n,&inj));s=n;
  assert(s.count==i+1&&s.pending&&inj.cr2==e.qualification&&inj.rip==e.rip);
  unchanged=n;
  assert(!lpfg_observe(&s,1ull<<31,e.rip,0,inj.info,&n));assert(!memcmp(&n,&unchanged,sizeof n));
  assert(!lpfg_observe(&s,0,e.rip,1ull<<31,inj.info,&n));
  assert(lpfg_observe(&s,0,e.rip,0,0,&n));assert(!n.pending);
  assert(!lpfg_observe(&s,0,e.rip,0,0x80000b0d,&n));
  assert(!lpfg_inject(&s,&e,0x1991000,0,&n,&inj));
  assert(lpfg_observe(&s,0,e.rip,0,0x80000b0e,&n));s=n;
  assert(!lpfg_inject(&s,&e,0x1991000,0,&n,&inj)); /* previous tuple */
 }
 e.qualification=0x1800000;assert(!lpfg_inject(&s,&e,0x1991000,0,&n,&inj));
 assert(lpfg_observe(&s,32,0x100001c,0,0,&n));assert(n.closed);s=n;
 assert(lpfg_observe(&s,0,e.rip,0,0,&n));assert(n.closed);
 s=(struct lpfg_state){.count=1,.seen=1};
 assert(lpfg_observe(&s,28,0x1977514,0,0,&n));assert(n.closed);s=n;

 assert(lpfg_observe(&s,28,0x100001c,0,0,&n));s=n;assert(s.closed);
 assert(lpfg_observe(&s,0,e.rip,0,0,&n));s=n;assert(s.closed);
 e.qualification=0x1200000;assert(!lpfg_inject(&s,&e,0x1991000,0,&n,&inj));
 s=(struct lpfg_state){0};e.qualification=0x1000000;
 assert(!lpfg_observe(&s,0,e.rip,0,0x80000b0e,&n));
 #define BAD(field,v) do{uint64_t old=e.field;e.field=(v);struct lpfg_state saved_n=n,saved_s=s;struct lve_pf_entry saved_i=inj;assert(!lpfg_inject(&s,&e,0x1991000,0,&n,&inj));assert(!memcmp(&n,&saved_n,sizeof n)&&!memcmp(&s,&saved_s,sizeof s)&&!memcmp(&inj,&saved_i,sizeof inj));e.field=old;}while(0)
 BAD(qualification,0x1200000);BAD(qualification,0x1000001);BAD(rip,0x19737f6);
 BAD(idt_base,0x1979051);BAD(idt_limit,0x200);BAD(interruption_error,3);
 BAD(interruption_info,0x80000b0d);BAD(entry_info,0x80000b0e);BAD(vectoring_info,1ull<<31);
 BAD(interruptibility,1);BAD(reason,1ull<<31);
 assert(!lpfg_inject(&s,&e,0x1992000,0,&n,&inj));
 assert(!lpfg_inject(&s,&e,0x1991000,1,&n,&inj));
 s.closed=true;assert(!lpfg_inject(&s,&e,0x1991000,0,&n,&inj));
 s=(struct lpfg_state){.count=1,.seen=0};assert(!lpfg_observe(&s,0,e.rip,0,0,&n));
  assert(!lpfg_observe(&s,0,e.rip,0,0x80000b0d,&n));

 /* Only an outstanding injection permits the exact valid-cleared encoding. */
 s=(struct lpfg_state){.count=1,.seen=1,.pending=true};
 assert(lpfg_observe(&s,0,e.rip,0,0xb0e,&n));assert(!n.pending&&n.count==1);
 for(unsigned bit=0;bit<64;bit++){
  uint64_t bad=0xb0eULL^(1ULL<<bit);
  if(bit==31)continue; /* Exact still-valid encoding is already permitted. */
  struct lpfg_state old=n;
  assert(!lpfg_observe(&s,0,e.rip,0,bad,&n));assert(!memcmp(&old,&n,sizeof n));
 }
 unchanged=n;
 assert(!lpfg_observe(&s,1ULL<<31,e.rip,0,0xb0e,&n));assert(!memcmp(&unchanged,&n,sizeof n));
 assert(!lpfg_observe(&s,0,e.rip,1ULL<<31,0xb0e,&n));
 s.pending=false;assert(!lpfg_observe(&s,0,e.rip,0,0xb0e,&n));
 s=(struct lpfg_state){0};assert(!lpfg_observe(&s,0,e.rip,0,0xb0e,&n));
 puts("linux_pf_guard_v5: PASS");
}
