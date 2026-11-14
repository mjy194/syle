#include "firmware_irq_apic.h"
#include "guest_mmio_mov.h"
static const unsigned rm[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
static bool project(const struct fag_state*d,const struct ftis_session*s,struct guest_apic*out){
 if(!d||!s||!out||!fag_valid(d)||!s->ready||s->failed||s->busy||s->active_attempt||
 s->event.phase!=LEI_IDLE||!s->owner.ready||s->owner.terminal||s->owner.reserved||
 !(d->apic.irq.svr&0x100))return false;
 *out=d->apic;out->irq.irr[1]=s->owner.pending?1:0;out->irq.isr[1]=s->owner.in_service?1:0;return true;
}
bool fiap_select(const struct fag_state*d,const struct ftis_session*s,bool*ready){
 struct guest_apic p;if(!ready||!project(d,s,&p))return false;
 uint8_t v;*ready=guest_apic_select(&p,&v);return !*ready||v==32;
}
static enum fat_result fatal(struct ftis_session*s){s->failed=true;s->busy=false;fti_quarantine(&s->owner);return FAT_FATAL;}
enum fat_result fiap_mmio(struct fag_state*d,struct ftis_session*s,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,void*c,lei_write w){
 struct guest_apic p;
 if(!g||!m||!e||!w||!project(d,s,&p)||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||e->dr7!=0x400||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7)))return FAT_REJECTED;
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[rm[i]];
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(!n||!guest_mmio_mov_decode(bytes,n,e->rip,regs,&plan)||plan.width!=4||
 (!plan.write&&plan.reg==4)||!fmm_validate(m,plan.linear,4,plan.write,e->reason,e->qualification,
 e->linear,e->physical,e->vectoring,0xfee00000,4096))return FAT_REJECTED;
 uint64_t off=e->physical-0xfee00000,value=plan.store_value,newreg=0;
 bool eoi=off==0xb0&&plan.write,tpr=off==0x80&&plan.write;
 if(plan.write&&!eoi&&!tpr)return FAT_REJECTED;
 uint32_t v=(uint32_t)value;struct guest_apic_effect effect={0};
 if(!plan.write&&(off==0x320||off==0x380||off==0x390||off==0x3e0)){
  if(s->owner.last<s->owner.origin||s->owner.last-s->owner.origin>UINT64_MAX/10)return FAT_REJECTED;
  uint64_t elapsed=(s->owner.last-s->owner.origin)*10;
  if(s->owner.deadline_bus<=elapsed)return FAT_REJECTED;
  uint64_t count=s->owner.deadline_bus-elapsed;if(count>UINT32_MAX)return FAT_REJECTED;
  v=off==0x320?0x20020:off==0x380?10000000:off==0x3e0?0xb:(uint32_t)count;
 }else if(!guest_apic_mmio(&p,off,4,plan.write,&v,0,&effect))return FAT_REJECTED;
 if(effect.level||(effect.eoi&&effect.vector!=32)||
 (eoi&&effect.eoi!=s->owner.in_service))return FAT_REJECTED;
 if(!plan.write&&!guest_mmio_mov_load_value(&plan,regs[plan.reg],v,&newreg))return FAT_REJECTED;
 s->busy=true;
 if(((e->rflags&0x10000)&&(!w(c,0x6820,e->rflags&~UINT64_C(0x10000))||s->failed||!s->busy))||
 !w(c,0x681e,plan.next_rip)||s->failed||!s->busy)return fatal(s);
 s->busy=false;
 if(eoi&&!ftis_eoi(s,s->domain,s->vmcs))return fatal(s);
 if(tpr)d->apic.irq.tpr=p.irq.tpr;
 if(!plan.write)g[rm[plan.reg]]=newreg;
 return FAT_COMMITTED;
}
