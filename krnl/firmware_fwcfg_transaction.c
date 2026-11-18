#include "firmware_fwcfg_transaction.h"
#include "firmware_piix_pm.h"
#ifdef FFC_RAMFB
static bool dma_owned(void*x,uint64_t a,uint64_t n){
 struct fmm_memory*m=x;struct firmware_memory*f=m->memory;
 if(!n||a>=FWM_RAM_SIZE||n>FWM_RAM_SIZE-a)return false;
 for(uint64_t p=a/4096;p<=(a+n-1)/4096;p++)if(!fwm_owned_page(f,f->ram[p]))return false;
 return true;
}
static bool dma_read(void*x,uint64_t a,void*b,unsigned n){struct fmm_memory*m=x;return dma_owned(x,a,n)&&fwm_read_ram(m->memory,a,b,n);}
struct dma_context {const struct fmm_memory*m;uint64_t rip;};
static bool dma_guarded_write(void*x,uint64_t a,const void*b,unsigned n){
 struct dma_context*c=x;struct fft_plan p;
 return dma_owned((void*)c->m,a,n)&&fft_prepare(c->m,a,n,c->rip,&p)&&fwm_write_ram(c->m->memory,a,b,n);
}
static bool dma_guarded_read(void*x,uint64_t a,void*b,unsigned n){return dma_read((void*)((struct dma_context*)x)->m,a,b,n);}
static bool dma_guarded_owned(void*x,uint64_t a,uint64_t n){return dma_owned((void*)((struct dma_context*)x)->m,a,n);}
#endif
bool fwc_apply(struct fwc_state*s,uint64_t g[15],const struct fmm_memory*m,const struct fwc_exit*e,bool exclusive,void*ctx,bool(*ram_write)(void*,uint8_t*,const uint8_t*,unsigned),bool(*wr)(void*,uint32_t,uint64_t)){
 if(!s||s->failed||!exclusive)return false;
 s->stage=0;
 if(!g||!m||!e||!wr||!ram_write||!s->stream.ready||e->length<1||e->length>15||e->rip>UINT64_MAX-e->length||
 !(e->flags&2)||(e->flags&~UINT64_C(0x3d7fd7))||(e->flags&UINT64_C(0x10500))||e->blocking||e->pending_debug||e->dr7!=0x400||e->entry_event||e->vectoring)goto fail;
 s->stage=1;
 uint8_t code[15];unsigned n=(unsigned)fmm_fetch(m,e->rip,code);
 struct frfb_state graphics=s->graphics;
 struct ffc_state staged=s->stream;uint64_t rax=g[0],count=0,di=g[5];bool rep=false;struct fft_plan target={0};uint8_t data[64];
 s->stage=2;
 if(e->qualification==UINT64_C(0x5110038)){
  rep=true;
  if(!e->info_supported||((e->info>>7)&7)!=2||e->length!=2||n<2||code[0]!=0xf3||code[1]!=0x6c||
   (uint16_t)g[3]!=0x511||e->es_base||(e->es_ar&~UINT32_C(0xc001))!=0x92||e->linear!=di||g[2]>64)goto fail;
  count=g[2];if(di>UINT64_MAX-count)goto fail;
  s->stage=3;
  if(count&&!fft_prepare(m,di,(unsigned)count,e->rip,&target))goto fail;
  s->stage=4;
  for(unsigned i=0;i<count;i++){uint64_t value=0;if(!ffc_io(&staged,0x511,1,true,&value))goto fail;data[i]=(uint8_t)value;}
 } else {
  if(!fpp_encoding(e->qualification,e->length,code,n)||
    (!(e->qualification&64)&&(uint16_t)g[3]!=(uint16_t)(e->qualification>>16)))goto fail;
#ifdef FFC_RAMFB
  uint16_t port=(uint16_t)(e->qualification>>16);
  if(port==0x514||port==0x518){
   struct dma_context c={m,e->rip};unsigned size=e->qualification&7;
   if(size!=3||!frfb_io(&graphics,&staged,port,4,!!(e->qualification&8),&rax,&c,dma_guarded_read,dma_guarded_write,dma_guarded_owned))goto fail;
  }else
#endif
  if(!ffc_exit(&staged,e->qualification,&rax))goto fail;
 }
 s->stage=5;
 if(rep&&count&&!ram_write(ctx,target.bytes,data,(unsigned)count))goto fail;
 s->stage=6;
 if(!wr(ctx,0x681e,e->rip+e->length))goto fail;
 s->stage=7;s->stream=staged;s->graphics=graphics;if(rep){g[2]=0;g[5]=di+count;}else g[0]=rax;
 return true;
fail:s->failed=true;return false;
}
