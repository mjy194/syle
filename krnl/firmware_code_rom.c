#include "firmware_code_rom.h"
#include "guest_mmio_mov.h"
bool fcode_init(struct fcode_state*s,const struct firmware_memory*f){
 if(!s||s->ready||s->failed||s->address||s->original||s->stage)return false;
 uint64_t address;uint8_t original;
 if(!fmm_code_probe_candidate(f,&address,&original))return false;
 *s=(struct fcode_state){.address=address,.original=original,.ready=true};return true;
}
static const unsigned regmap[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
enum fat_result fcode_mmio(struct fcode_state*s,const uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,bool exclusive,
 void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!s->ready||s->failed||s->stage>1||!g||!m||!e||!write||!exclusive||
 s->address<UINT64_C(0xffc84000)||s->address-UINT64_C(0xffc84000)>=65536||
 s->original==0||s->original==0x50||s->original==0x70||
 e->ds_base||e->es_base||e->ss_base||e->vectoring||e->entry_event||e->blocking||e->pending_debug||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7))||e->dr7!=0x400)return FAT_REJECTED;
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[regmap[i]];
 uint8_t bytes[15];size_t available=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,regs,&plan)||
 !plan.write||plan.width!=1||plan.store_value!=(s->stage?0x70u:0x50u)||
 e->physical!=s->address||
 !fmm_validate_code_write(m,plan.linear,plan.width,e->reason,e->qualification,e->linear,e->physical,e->vectoring))return FAT_REJECTED;
 uint8_t original;
 if(!fmm_read_code_byte(m->memory,s->address,&original)||original!=s->original)return FAT_REJECTED;
 if(((e->rflags&(UINT64_C(1)<<16))&&!write(ctx,0x6820,e->rflags&~(UINT64_C(1)<<16)))||
 !write(ctx,0x681e,plan.next_rip)){s->failed=true;return FAT_FATAL;}
 s->stage++;return FAT_COMMITTED;
}
