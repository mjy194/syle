#include "firmware_tsc_read.h"
static enum fat_result fatal(struct ftsc_state*s){s->failed=true;s->busy=false;return FAT_FATAL;}
static bool canonical(uint64_t a){return a<=UINT64_C(0x7fffffffffff)||a>=UINT64_C(0xffff800000000000);}
bool ftsc_init(struct ftsc_state*s,uint64_t d,uint64_t e,uint64_t r,uint64_t gap,uint32_t cpu){
 if(!s||s->ready||s->failed||s->busy||s->domain||s->epoch||s->origin||s->last||s->max_gap||!d||!e||!gap)return false;
 *s=(struct ftsc_state){.domain=d,.epoch=e,.origin=r,.last=r,.max_gap=gap,.cpu=cpu,.ready=true};return true;
}
bool ftsc_sample(struct ftsc_state*s,uint64_t d,uint64_t e,uint32_t cpu,uint64_t r){
 if(!s||!s->ready||s->failed)return false;
 if(s->busy||d!=s->domain||e!=s->epoch||cpu!=s->cpu||r<s->last||
 r<s->origin||r-s->last>s->max_gap){fatal(s);return false;}
 s->last=r;return true;
}
enum fat_result ftsc_read(struct ftsc_state*s,const struct ftsc_sample*t,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,unsigned len,bool exclusive,void*c,lei_write w){
 if(!exclusive)return FAT_REJECTED;
 if(s&&s->busy)return fatal(s);
 if(!s||!s->ready||s->failed)return FAT_REJECTED;
 if(!t||!t->source_admitted||t->domain!=s->domain||t->epoch!=s->epoch||
 t->cpu!=s->cpu||t->reference!=s->last||t->reference<s->origin)return fatal(s);
 if(!g||!m||!e||!w||len!=2||e->reason!=16||e->qualification||e->vectoring||
 e->entry_event||e->blocking||e->pending_debug||e->dr7!=0x400||!(e->rflags&2)||
 (e->rflags&~UINT64_C(0x250ed7))||!canonical(e->rip)||e->rip>UINT64_MAX-2||!canonical(e->rip+2))return FAT_REJECTED;
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);
 if(n<2||bytes[0]!=0x0f||bytes[1]!=0x31)return FAT_REJECTED;
 uint64_t value=t->reference-s->origin;s->busy=true;
 if(((e->rflags&0x10000)&&(!w(c,0x6820,e->rflags&~UINT64_C(0x10000))||s->failed||!s->busy))||
 !w(c,0x681e,e->rip+2)||s->failed||!s->busy)return fatal(s);
 s->busy=false;g[0]=(uint32_t)value;g[3]=(uint32_t)(value>>32);return FAT_COMMITTED;
}
