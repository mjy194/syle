#include "../firmware_console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool fail;static uint64_t savedrip;
static bool wr(void*x,uint32_t f,uint64_t v){(void)x;assert(f==0x681e);savedrip=v;return !fail;}
static void cfg(struct fcon_state*s,unsigned o,unsigned w,uint64_t v){assert(fcon_config(s,o,w,false,&v));}
static uint64_t rd(struct fcon_state*s,unsigned o,unsigned w){uint64_t v=~0ULL;assert(fcon_config(s,o,w,true,&v));return v;}
static bool io(struct fcon_state*s,uint64_t*g,unsigned r,bool input,bool*emit,uint8_t*b){g[3]=s->bar+r;uint8_t op=input?0xec:0xee;return fcon_io(s,g,0x1000,(g[3]<<16)|(input?8:0),1,&op,1,0,wr,emit,b);}
int main(void){struct fcon_state s={0};uint64_t g[15]={0};bool emit;uint8_t b;
 assert(rd(&s,0,4)==0x00021b36);assert(rd(&s,8,4)==0x07000200);
 cfg(&s,16,4,~0u);assert(rd(&s,16,4)==0xfffffff9);assert(!fcon_port(&s,0xc000));
 cfg(&s,16,4,0xc001);cfg(&s,4,2,7);assert(rd(&s,4,2)%65536==1);assert(fcon_port(&s,0xc007)&&!fcon_port(&s,0xc008));
 g[0]=3;assert(io(&s,g,3,false,&emit,&b)&&!emit);
 for(unsigned v=0;v<256;v++){g[0]=v;assert(io(&s,g,0,false,&emit,&b)&&emit&&b==v&&savedrip==0x1001);assert(fcon_receive(&s,(uint8_t)v));g[0]=0x123400;assert(io(&s,g,5,true,&emit,&b)&&(g[0]&1));assert(io(&s,g,0,true,&emit,&b)&&g[0]==(0x123400|v));}
 assert(fcon_receive(&s,42));struct fcon_state old=s;assert(!fcon_receive(&s,43)&&!memcmp(&old,&s,sizeof s));
 assert(io(&s,g,0,true,&emit,&b)&&((g[0]&255)==42));
 g[0]=0x83;assert(io(&s,g,3,false,&emit,&b));g[0]=1;assert(io(&s,g,1,false,&emit,&b));assert(s.uart.dlm==1);
 g[0]=3;assert(io(&s,g,3,false,&emit,&b));g[0]=1;old=s;assert(!io(&s,g,1,false,&emit,&b)&&!memcmp(&old,&s,sizeof s));
 uint8_t op=0xed;g[3]=s.bar;assert(!fcon_io(&s,g,0x1000,(uint64_t)s.bar<<16,1,&op,1,0,wr,&emit,&b));
 g[0]='X';old=s;uint64_t oldrax=g[0];fail=true;assert(!io(&s,g,0,false,&emit,&b)&&s.failed&&g[0]==oldrax&&s.uart.tc==old.uart.tc);fail=false;assert(!io(&s,g,0,false,&emit,&b));
 puts("firmware PCI serial config / 256-byte TX RX / decode gating / atomic commit PASS");}
