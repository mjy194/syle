#include "firmware_vars_rom.h"
#include "guest_mmio_mov.h"
static bool layout(const struct firmware_memory*f){
 return f&&f->layout.ram.base==0&&f->layout.ram.size==FWM_RAM_SIZE&&
 f->layout.vars.base==UINT64_C(0xffc00000)&&f->layout.vars.size==540672&&
 f->layout.code.base==UINT64_C(0xffc84000)&&f->layout.code.size==3653632;
}
bool fvr_init(struct fvr_state*s,const struct firmware_memory*f){
 if(!s||s->ready||s->failed||s->address||s->original||s->stage||!layout(f))return false;
 for(unsigned i=0;i<65536;i++){
  uint8_t v;uint64_t address=f->layout.vars.base+i;if(!fmm_read_vars_byte(f,address,&v))return false;
  if(v!=0&&v!=0x50&&v!=0x70){*s=(struct fvr_state){.address=address,.original=v,.ready=true};return true;}
 }
 return false;
}
bool fvr_probe(struct fvr_state*s,const struct firmware_memory*f,uint64_t gpa,unsigned width,uint64_t value){
 if(!s||!s->ready||s->failed||!layout(f)||width!=1||gpa!=s->address||
 s->address<f->layout.vars.base||s->address-f->layout.vars.base>=65536||
 s->original==0||s->original==0x50||s->original==0x70||s->stage>1||value!=(s->stage?0x70u:0x50u))return false;
 uint8_t original;if(!fmm_read_vars_byte(f,gpa,&original)||original!=s->original)return false;
 s->stage++;return true;
}
static const unsigned regmap[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
enum fat_result fvr_mmio_traced(struct fvr_state*s,uint64_t g[15],const struct fmm_memory*m,
 const struct fat_exit*e,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t),struct fvr_trace*t){
 if(t)*t=(struct fvr_trace){0};
 if(!s||!s->ready||s->failed||!g||!m||!e||!write||!exclusive||
 e->ds_base||e->es_base||e->ss_base||e->vectoring||e->entry_event||e->blocking||e->pending_debug||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7))||e->dr7!=0x400)return FAT_REJECTED;
 if(t)t->stage=1;
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[regmap[i]];
 uint8_t bytes[15];size_t available=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(t){t->available=(unsigned)available;t->stage=2;}
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,regs,&plan))return FAT_REJECTED;
 if(t){t->stage=3;t->width=plan.width;t->write=plan.write;t->linear=plan.linear;t->value=plan.store_value;}
 if(!plan.write||plan.width!=1||!fmm_validate_vars_write(m,plan.linear,plan.width,e->reason,e->qualification,e->linear,e->physical,e->vectoring))return FAT_REJECTED;
 if(t)t->stage=4;
 struct fvr_state staged=*s;
 if(!fvr_probe(&staged,m->memory,e->physical,plan.width,plan.store_value))return FAT_REJECTED;
 if(t)t->stage=5;
 if(((e->rflags&(UINT64_C(1)<<16))&&!write(ctx,0x6820,e->rflags&~(UINT64_C(1)<<16)))||
 !write(ctx,0x681e,plan.next_rip)){s->failed=true;return FAT_FATAL;}
 *s=staged;if(t)t->stage=6;return FAT_COMMITTED;
}

enum fat_result fvr_mmio(struct fvr_state*s,uint64_t g[15],const struct fmm_memory*m,
 const struct fat_exit*e,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 return fvr_mmio_traced(s,g,m,e,exclusive,ctx,write,0);
}
