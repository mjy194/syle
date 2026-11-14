#include "firmware_irq_cr8_write.h"
static enum fat_result fatal(struct ftis_session*s){s->failed=true;s->busy=false;fti_quarantine(&s->owner);return FAT_FATAL;}
static bool canonical(uint64_t a){return a<=UINT64_C(0x7fffffffffff)||a>=UINT64_C(0xffff800000000000);}
enum fat_result fir8_write(struct fag_state*d,struct ftis_session*s,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,unsigned len,bool exclusive,
 void*c,lei_write w){
 if(!exclusive)return FAT_REJECTED;
 if(s&&s->busy)return fatal(s);
 bool ready;
 if(!g||!m||!e||!w||!fiap_select(d,s,&ready)||e->reason!=28||
 e->qualification!=0x8||len!=4||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||e->dr7!=0x400||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7))||
 !canonical(e->rip)||e->rip>UINT64_MAX-4||!canonical(e->rip+4))return FAT_REJECTED;
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);
 if(n<4||bytes[0]!=0x44||bytes[1]!=0x0f||bytes[2]!=0x22||bytes[3]!=0xc0)return FAT_REJECTED;
 if(g[0]>15)return FAT_REJECTED;
 uint8_t value=(uint8_t)(g[0]<<4);
 s->busy=true;
 if(((e->rflags&0x10000)&&(!w(c,0x6820,e->rflags&~UINT64_C(0x10000))||s->failed||!s->busy))||
 !w(c,0x681e,e->rip+4)||s->failed||!s->busy)return fatal(s);
 s->busy=false;d->apic.irq.tpr=value;return FAT_COMMITTED;
}
