#include "linux_vcpu_event.h"
static bool canonical(uint64_t a){return (a>>47)==0||(a>>47)==0x1ffff;}
bool lve_demand_pf(const struct lve_pf_exit *e,struct lve_pf_entry *out){
 if(!e||!out||e->reason||e->interruption_info!=0x80000b0eULL||
 e->interruption_error!=2||e->vectoring_info||e->entry_info||
 e->interruptibility||e->qualification>=128ULL*1024*1024||
 !canonical(e->rip)||!e->idt_base||!canonical(e->idt_base)||
 e->idt_limit<239||e->idt_limit>65535||
 e->idt_base>UINT64_MAX-e->idt_limit||!canonical(e->idt_base+e->idt_limit))return false;
 *out=(struct lve_pf_entry){0x80000b0eULL,2,0,e->qualification,e->rip};return true;
}
