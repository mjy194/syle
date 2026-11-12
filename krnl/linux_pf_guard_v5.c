/* v5: exact observed consumed event encoding; legacy guard remains unchanged. */
#include "linux_pf_guard.h"
static bool valid(const struct lpfg_state*s){return s&&s->count<=4&&s->seen==((1u<<s->count)-1)&&(!s->pending||s->count);}
bool lpfg_observe(const struct lpfg_state*s,uint64_t reason,uint64_t rip,uint64_t vectoring,uint64_t event,struct lpfg_state*out){
 if(!valid(s)||!out||(reason&(1ull<<31))||(vectoring&(1ull<<31))||
    (s->pending?(event!=0&&event!=0x80000b0eULL&&event!=0xb0eULL):event!=0))return false;
 struct lpfg_state n=*s;n.pending=false;
 /* Deliberately conservative diagnostic fence: any other RIP after the first
  * PF closes injection forever, including possible handler-internal CR exits.
  * This may stop early and does not claim complete handler-path support. */
 if(n.count&&rip!=0x19737f5ULL)n.closed=true;
 *out=n;return true;
}
bool lpfg_inject(const struct lpfg_state*s,const struct lve_pf_exit*e,uint64_t cr3,uint64_t activity,struct lpfg_state*out,struct lve_pf_entry*injection){
 if(!valid(s)||!e||!out||!injection||s->closed||s->pending||s->count==4||activity||
    e->rip!=0x19737f5ULL||e->idt_base!=0x1979050ULL||e->idt_limit!=0x1ff||
    cr3!=0x1991000ULL||e->qualification!=0x1000000ULL+(uint64_t)s->count*0x200000||
    (s->seen&(1u<<s->count)))return false;
 struct lve_pf_entry p;if(!lve_demand_pf(e,&p))return false;
 struct lpfg_state n=*s;n.seen|=1u<<n.count;n.count++;n.pending=true;
 *out=n;*injection=p;return true;
}
