#include "firmware_irq_lvt_same.h"
#include "guest_mmio_mov.h"
static const unsigned rm[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
static enum fat_result fatal(struct ftis_session*s){s->failed=true;s->busy=false;fti_quarantine(&s->owner);return FAT_FATAL;}
enum fat_result fils_write(struct fag_state*d,struct ftis_session*s,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,bool exclusive,void*c,lei_write w){
 if(!exclusive)return FAT_REJECTED;
 if(s&&s->busy)return fatal(s);
 bool ready;
 if(!g||!m||!e||!w||!fiap_select(d,s,&ready)||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||e->dr7!=0x400||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7))||e->physical!=0xfee00320)return FAT_REJECTED;
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[rm[i]];
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(!n||!guest_mmio_mov_decode(bytes,n,e->rip,regs,&plan)||!plan.write||plan.width!=4||
 plan.store_value!=0x20020||!fmm_validate(m,plan.linear,4,true,e->reason,e->qualification,
 e->linear,e->physical,e->vectoring,0xfee00320,4))return FAT_REJECTED;
 s->busy=true;
 if(((e->rflags&0x10000)&&(!w(c,0x6820,e->rflags&~UINT64_C(0x10000))||s->failed||!s->busy))||
 !w(c,0x681e,plan.next_rip)||s->failed||!s->busy)return fatal(s);
 s->busy=false;return FAT_COMMITTED;
}
