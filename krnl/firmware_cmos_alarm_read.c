#include "firmware_cmos_alarm_read.h"
static bool canonical(uint64_t a){return a<=UINT64_C(0x7fffffffffff)||a>=UINT64_C(0xffff800000000000);}
static enum fat_result fatal(struct fcra_guard*b,struct fcr_state*s){b->failed=true;b->busy=false;if(s)s->failed=true;return FAT_FATAL;}
enum fat_result fcra_read(struct fcra_guard*b,struct fcr_state*s,uint64_t g[15],
 const struct fat_exit*e,unsigned len,unsigned cpl,const uint8_t*bytes,size_t n,
 bool exclusive,void*c,lei_write w){
 if(!exclusive)return FAT_REJECTED;
 if(b&&b->busy)return fatal(b,s);
 if(!b||b->failed||!s||!s->ready||s->failed||s->rtc.failed||!s->memory.ready||
 s->memory.above16_low||s->memory.above16_high!=7||s->rtc.index>127||
 (s->rtc.status_c&~0x50)||!s->epoch||s->last<s->origin||!g||!e||!bytes||!n||!w||
 cpl||len!=1||bytes[0]!=0xec||e->reason!=30||e->qualification!=0x710008||
 (uint16_t)g[3]!=0x71||(s->rtc.index!=1&&s->rtc.index!=3&&s->rtc.index!=5)||e->vectoring||e->entry_event||e->blocking||
 e->pending_debug||e->dr7!=0x400||!(e->rflags&2)||
 (e->rflags&~UINT64_C(0x250ed7))||!canonical(e->rip)||
 e->rip==UINT64_MAX||!canonical(e->rip+1))return FAT_REJECTED;
 uint64_t value=g[0]&~UINT64_C(255);
 b->busy=true;
 if(((e->rflags&0x10000)&&(!w(c,0x6820,e->rflags&~UINT64_C(0x10000))||b->failed||!b->busy||s->failed))||
 !w(c,0x681e,e->rip+1)||b->failed||!b->busy||s->failed)return fatal(b,s);
 b->busy=false;g[0]=value;return FAT_COMMITTED;
}
