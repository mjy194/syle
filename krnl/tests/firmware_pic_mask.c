#include "../linux_pic_mask.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls;static uint64_t next;static bool fail;
static bool wr(void*x,uint32_t field,uint64_t v){(void)x;assert(field==0x681e);calls++;next=v;return !fail;}
static bool io(struct lpic_state*s,unsigned port,unsigned val,bool input,uint64_t flags){uint64_t g[15]={0};g[0]=0xaabbccdd00000000ull|val;g[3]=port;unsigned prev=calls;bool ok=lpic_io_flags(s,g,0xfffcea87,0,30,((uint64_t)port<<16)|(input?8:0),1,flags,0,wr);if(ok){assert(calls==prev+1&&next==0xfffcea88);assert((g[0]>>32)==0xaabbccdd);}return ok;}
int main(void){struct lpic_state s;lpic_init(&s);assert(s.master==255&&s.slave==255);
 assert(io(&s,0x21,255,false,0)&&io(&s,0xa1,255,false,0));assert(io(&s,0x21,0,true,512));
 assert(io(&s,0x20,0x11,false,0));assert(!lpic_entry_ok(&s,512));
 assert(io(&s,0x21,0x68,false,0));assert(io(&s,0x21,4,false,0));assert(io(&s,0x21,1,false,0));assert(!lpic_entry_ok(&s,512));
 assert(io(&s,0x21,255,false,0)&&lpic_entry_ok(&s,512));
 assert(!io(&s,0x20,0x20,false,0)&&s.failed);lpic_init(&s);
 unsigned prev=calls;assert(!io(&s,0x21,0xfe,false,0)&&s.failed&&calls==prev);lpic_init(&s);
 fail=true;assert(!io(&s,0x21,0xfb,false,0)&&s.failed&&s.master==255);fail=false;
 lpic_init(&s);uint64_t g[15]={0};g[0]=255;g[3]=0x21;
 assert(!lpic_io_flags(&s,g,0xfffcea87,0,30,0x00210003,1,0,0,wr)&&s.failed);
 lpic_init(&s);assert(s.master==255&&s.slave==255&&!s.failed&&!s.chip[0].started);
 puts("firmware PIC reuse: observed mask/init/IF/commit failure/reset PASS");}
