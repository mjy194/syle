#include "firmware_tpm_transaction.h"
#include "firmware_tpm_absent.h"
#include "guest_mmio_mov.h"
static const unsigned regmap[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
enum fat_result ftt_mmio(struct ftt_state*s,uint64_t g[15],const struct fmm_memory*m,
 const struct fat_exit*e,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||s->failed||!g||!m||!e||!write||!exclusive||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7))||e->dr7!=0x400)return FAT_REJECTED;
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[regmap[i]];
 uint8_t bytes[15];size_t available=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,regs,&plan)||plan.write||plan.width!=1||
 plan.reg>=16||plan.reg==4||!fmm_validate(m,plan.linear,1,false,e->reason,e->qualification,e->linear,e->physical,
 e->vectoring,UINT64_C(0xfed40000),1))return FAT_REJECTED;
 uint64_t value=0,newreg;
 if(!fta_mmio(e->physical,1,false,&value)||!guest_mmio_mov_load_value(&plan,regs[plan.reg],value,&newreg))return FAT_REJECTED;
 if(((e->rflags&(UINT64_C(1)<<16))&&!write(ctx,0x6820,e->rflags&~(UINT64_C(1)<<16)))||
 !write(ctx,0x681e,plan.next_rip)){s->failed=true;return FAT_FATAL;}
 g[regmap[plan.reg]]=newreg;return FAT_COMMITTED;
}
