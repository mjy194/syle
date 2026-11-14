#include "firmware_debug_read.h"
static enum fat_result fatal(struct fdr_bank*b){b->failed=true;b->busy=false;return FAT_FATAL;}
static bool canonical(uint64_t a){return a<=UINT64_C(0x7fffffffffff)||a>=UINT64_C(0xffff800000000000);}
bool fdr_init(struct fdr_bank*b,uint64_t d,uint64_t v){
 if(!b||b->ready||b->failed||b->busy||b->domain||b->vmcs||!d||!v)return false;
 *b=(struct fdr_bank){.domain=d,.vmcs=v,.ready=true};return true;
}
enum fat_result fdr_read(struct fdr_bank*b,const struct fdr_gate*gate,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,unsigned len,bool exclusive,void*c,lei_write w){
 if(!exclusive)return FAT_REJECTED;
 if(b&&b->busy)return fatal(b);
 if(!b||!b->ready||b->failed||!gate||!g||!m||!e||!w||
 gate->domain!=b->domain||gate->vmcs!=b->vmcs||!gate->debug_events_absent||
 gate->primary_controls>>32||!(gate->primary_controls&(UINT64_C(1)<<23))||
 gate->debugctl||e->reason!=29||len!=3||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||e->dr7!=0x400||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7))||
 !canonical(e->rip)||e->rip>UINT64_MAX-3||!canonical(e->rip+3))return FAT_REJECTED;
 uint64_t dr=e->qualification&7;
 if(e->qualification!=(0x10|dr)||dr==4||dr==5)return FAT_REJECTED;
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);
 if(n<3||bytes[0]!=0x0f||bytes[1]!=0x21||bytes[2]!=(0xc0|(dr<<3)))return FAT_REJECTED;
 uint64_t value=dr==6?UINT64_C(0xffff0ff0):dr==7?0x400:0;
 b->busy=true;
 if(((e->rflags&0x10000)&&(!w(c,0x6820,e->rflags&~UINT64_C(0x10000))||b->failed||!b->busy))||
 !w(c,0x681e,e->rip+3)||b->failed||!b->busy)return fatal(b);
 b->busy=false;g[0]=value;return FAT_COMMITTED;
}
