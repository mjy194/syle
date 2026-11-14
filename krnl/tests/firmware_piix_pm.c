#include "../firmware_piix_pm.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool fail;static unsigned calls;static uint64_t expected_rip=0x1001;
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;assert(f==0x681e&&v==expected_rip);calls++;return !fail;}
static bool io(struct fpp_state*s,uint64_t*g,unsigned p,unsigned size,bool in,uint64_t ref){unsigned length=size==1?2:1;expected_rip=0x1000+length;uint8_t bytes[2]={0};unsigned at=0;if(size==1)bytes[at++]=0x66;bytes[at]=(uint8_t)(0xec+(in?0:2)+(size==0?0:1));g[3]=p;return fpp_io(s,g,0x1000,0,((uint64_t)p<<16)|size|(in?8:0),length,1,ref,bytes,length,true,0,wr);}
static void addr(struct fpp_state*s,uint64_t*g,uint32_t a){g[0]=a;assert(io(s,g,0xcf8,3,false,s->pm.last));}
int main(void){const uint8_t ed[]={0xed};struct fpp_state s={0};uint64_t g[15]={0};assert(fpp_init(&s,1,0,UINT64_MAX));
 /* Real OVMF absent CPU hotplug probe, no modern device invented. */
 struct fpp_state hp={0};assert(fpp_init(&hp,1,0,UINT64_MAX));g[0]=0;
 assert(io(&hp,g,0xaf00,3,false,0));assert(io(&hp,g,0xaf00,3,false,0));
 assert(io(&hp,g,0xaf05,0,false,0));g[0]=UINT64_MAX;
 assert(io(&hp,g,0xaf00,3,true,0)&&g[0]==UINT32_MAX);
 struct fpp_state hpfail=hp;g[0]=0x123456;calls=0;fail=true;
 assert(!io(&hpfail,g,0xaf00,3,true,0)&&hpfail.failed&&g[0]==0x123456&&calls==1);
 fail=false;calls=0;assert(!io(&hpfail,g,0xaf00,3,true,0)&&!calls);
 struct fpp_state hpbad=hp;g[0]=1;calls=0;
 assert(!io(&hpbad,g,0xaf05,0,false,0)&&hpbad.failed&&!calls&&g[0]==1);
 /* Fixed A20 gate: real IoOr8 read-modify-write; no reset service. */
 struct fpp_state a20={0};assert(fpp_init(&a20,1,0,UINT64_MAX));g[0]=0x1237;
 assert(io(&a20,g,0x92,0,true,0)&&g[0]==0x1202);
 g[0]|=2;assert(io(&a20,g,0x92,0,false,0)&&g[0]==0x1202);
 for(unsigned v=0;v<256;v++)if(v!=2){
  struct fpp_state bad=a20;g[0]=v;calls=0;
  assert(!io(&bad,g,0x92,0,false,0)&&bad.failed&&!calls&&g[0]==v);
 }
 struct fpp_state a20fail=a20;g[0]=0x1237;fail=true;calls=0;
 assert(!io(&a20fail,g,0x92,0,true,0)&&a20fail.failed&&g[0]==0x1237&&calls==1);
 fail=false;calls=0;assert(!io(&a20fail,g,0x92,0,true,0)&&!calls);
 /* Exact native regression: CF8=80000000; IN AX,DX bytes66 ED, CFE. */
 addr(&s,g,0x80000000);g[0]=0x81fd08;
 assert(io(&s,g,0xcfe,1,true,0)&&g[0]==0x811237&&expected_rip==0x1002);
 addr(&s,g,0x80000b80);g[0]=UINT64_MAX;assert(io(&s,g,0xcfc,0,true,0)&&g[0]==UINT64_C(0xffffffffffffff00));
 addr(&s,g,0x80000b40);assert(io(&s,g,0xcfc,3,true,0)&&g[0]==1);
 g[0]=0xb001;assert(io(&s,g,0xcfc,3,false,0));addr(&s,g,0x80000b80);g[0]=1;assert(io(&s,g,0xcfc,0,false,0));
 assert(io(&s,g,0xb008,3,true,10000000)&&g[0]==3579545);
 struct fpp_state saved=s;g[0]=0;fail=true;calls=0;
 assert(!io(&s,g,0xcfc,0,false,10000000)&&s.failed&&s.pm.enable==saved.pm.enable&&g[0]==0&&calls==1);fail=false;
 struct fpp_state x={0};assert(fpp_init(&x,1,0,UINT64_MAX));addr(&x,g,0x80000b00);g[0]=123;calls=0;assert(!io(&x,g,0xcfc,3,true,0)&&x.failed&&g[0]==123&&!calls);
 struct fpp_state absent={0};assert(fpp_init(&absent,1,0,UINT64_MAX));addr(&absent,g,0x80001000);assert(io(&absent,g,0xcfc,3,true,0)&&g[0]==0xffffffff);
 struct fpp_state busy={0};assert(fpp_init(&busy,1,0,UINT64_MAX));saved=busy;calls=0;
 assert(!fpp_io(&busy,g,0x1000,0,0xcf80003,1,1,100000000,ed,1,false,0,wr)&&!memcmp(&busy,&saved,sizeof busy)&&!calls);
 struct fpp_state wrong={0};assert(fpp_init(&wrong,1,0,UINT64_MAX));calls=0;assert(!io(&wrong,g,6,3,true,0)&&wrong.failed&&!calls);
 struct fpp_state invalid={0};assert(fpp_init(&invalid,1,0,UINT64_MAX));calls=0;g[3]=0xcf8;
 assert(!fpp_io(&invalid,g,0x1000,0,0xcf800043,2,1,0,ed,1,true,0,wr)&&invalid.failed&&!calls);
 struct fpp_state cross={0};assert(fpp_init(&cross,1,0,UINT64_MAX));addr(&cross,g,0x80000b40);calls=0;
 assert(!io(&cross,g,0xcfd,3,true,0)&&cross.failed&&!calls);
 struct fpp_state clock={0};assert(fpp_init(&clock,1,100,10));calls=0;g[3]=0xcf8;
 assert(!fpp_io(&clock,g,0x1000,0,0xcf800003,1,1,111,ed,1,true,0,wr)&&clock.failed&&clock.pm.failed&&!calls);
 struct fpp_state lenbad={0};assert(fpp_init(&lenbad,1,0,UINT64_MAX));addr(&lenbad,g,0x80000000);calls=0;g[3]=0xcfe;
 assert(!fpp_io(&lenbad,g,0x1000,0,0xcfe0009,1,1,0,ed,1,true,0,wr)&&lenbad.failed&&!calls);
 struct fpp_state olddword={0};assert(fpp_init(&olddword,1,0,UINT64_MAX));addr(&olddword,g,0x80000000);
 assert(io(&olddword,g,0xcfc,3,true,0)&&g[0]==0x12378086&&expected_rip==0x1001);
 const uint8_t word[]={0x66,0xed},addrprefix[]={0x67,0xed},repeat[]={0x66,0x66,0xed},rep[]={0xf3,0xed},rex[]={0x48,0xed},out[]={0x66,0xef};
 assert(fpp_encoding(0xcfe0009,2,word,2));assert(!fpp_encoding(0xcfe0009,1,word,2));
 assert(!fpp_encoding(0xcfe000b,2,addrprefix,2));assert(!fpp_encoding(0xcfe0009,3,repeat,3));
 assert(!fpp_encoding(0xcfe000b,2,rep,2));assert(!fpp_encoding(0xcfe000b,2,rex,2));
 assert(!fpp_encoding(0xcfe0009,2,out,2));assert(fpp_encoding(0xcfe0001,2,out,2));
 const uint8_t immediate[]={0x66,0xe5,0x80};assert(fpp_encoding(0x800049,3,immediate,3));
 assert(!fpp_encoding(0x810049,3,immediate,3));assert(!fpp_encoding(0x800049,3,immediate,2));
 puts("firmware PIIX PM: discovery/PMBA/decode, atomic RIP, ownership PASS");}
