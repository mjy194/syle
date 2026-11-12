#include "../krnl/linux_vcpu_event.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 struct lve_pf_exit e={.qualification=0x1000000,.rip=0x19737f5,
 .interruption_info=0x80000b0e,.interruption_error=2,.idt_base=0x1979050,.idt_limit=511};
 struct lve_pf_entry out={0},sentinel;
 assert(lve_demand_pf(&e,&out));assert(out.cr2==0x1000000&&out.rip==e.rip&&out.info==0x80000b0e&&out.error==2&&!out.instruction_length);
 for(unsigned i=0;i<12;i++){
 struct lve_pf_exit bad=e;
 switch(i){case 0:bad.reason=1;break;case 1:bad.interruption_info^=1;break;
 case 2:bad.interruption_error=3;break;case 3:bad.vectoring_info=1;break;
 case 4:bad.entry_info=1;break;case 5:bad.interruptibility=1;break;
 case 6:bad.qualification=128ULL*1024*1024;break;case 7:bad.rip=1ULL<<48;break;
 case 8:bad.idt_base=0;break;case 9:bad.idt_limit=238;break;
 case 10:bad.idt_limit=65536;break;case 11:bad.idt_base=UINT64_MAX-1;break;}
 memset(&out,0xa5,sizeof out);sentinel=out;
 assert(!lve_demand_pf(&bad,&out));assert(!memcmp(&out,&sentinel,sizeof out));
 }
 assert(!lve_demand_pf(0,&out)&&!lve_demand_pf(&e,0));puts("reconstructed demand PF policy: PASS; no VM tested");
}
