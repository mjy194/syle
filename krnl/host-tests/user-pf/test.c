#include "linux_user_pf.h"
#include "linux_guest_session.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char mem[65536];static struct lgs_session session;static struct lup_snapshot g;static struct lnp_state state;static uint64_t fields[0x7000];static unsigned reads,writes,failread,failwrite;static uint64_t missing;
#define K UINT64_C(0xffff800000000000)
static void put(uint64_t a,uint64_t v){assert(a<=sizeof mem-8);for(unsigned i=0;i<8;i++)mem[a+i]=v>>(8*i);}
static bool readmem(void*c,uint64_t a,void*b,size_t n){(void)c;if(a>=sizeof mem||n>sizeof mem-a||(missing&&a/4096==missing))return false;memcpy(b,mem+a,n);return true;}
static void*map(void*c,uint64_t p){(void)c;return p<sizeof mem&&p/4096!=missing?mem+p:0;}
static bool rd(void*c,uint32_t f,uint64_t*v){(void)c;if(++reads==failread)return false;*v=fields[f];return true;}
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;fields[f]=v;return ++writes!=failwrite;}
static void setup(void){
 memset(mem,0,sizeof mem);memset(&session,0,sizeof session);memset(fields,0,sizeof fields);state=(struct lnp_state){0};missing=reads=writes=failread=failwrite=0;
 g=(struct lup_snapshot){.info=0x80000b0e,.error=20,.address=0x400000,.rip=0x400000,.rsp=0x7ffd42a72f40,.cr0=0x80050033,.cr3=0x1000,.cr4=0x26b0,.efer=0xd01,.cs=0x33,.cslimit=UINT32_MAX,.csar=0xa0fb,.ss=0x2b,.sslimit=UINT32_MAX,.ssar=0xc0f3,.idtr=K+0x5000,.idtlimit=4095,.gdtr=K+0x6000,.gdtlimit=127,.tr=0x40,.trbase=K+0x9000,.trlimit=103,.trar=0x8b,.flags=0x10202};
 put(0x1000,0x2007);put(0x2000,0x3007);put(0x3010,0x4007); /* A=0 accepted, final PTE absent */
 put(0x1800,0xb003);put(0xb000,0xc003);put(0xc000,0xd003);
 for(unsigned i=5;i<=10;i++)put(0xd000+i*8,(i*4096)|1|(i==8?2:0)|(i==10?0:UINT64_C(1)<<63));
 put(0x50e0,UINT64_C(0x00008e000010a000));put(0x50e8,0xffff8000);
 put(0x6010,UINT64_C(0x00af9b000000ffff));put(0x6040,UINT64_C(0x00008b0090000067));put(0x6048,0xffff8000);put(0x9004,K+0x9000);
 session.prepared=session.memory.prepared=session.host.active=session.host.ready=session.state.ready=session.state.guest_valid=true;
 session.memory.owner.map=map;session.platform.read_field=rd;session.hop.vmwrite=wr;
 for(unsigned i=1;i<16;i++)session.memory.pages[i]=i*4096;
 uint64_t vals[]={g.reason,g.info,g.error,g.address,g.rip,g.rsp,g.cr0,g.cr3,g.cr4,g.efer,g.cs,g.csbase,g.cslimit,g.csar,g.ss,g.ssbase,g.sslimit,g.ssar,g.idtr,g.idtlimit,g.gdtr,g.gdtlimit,g.tr,g.trbase,g.trlimit,g.trar,g.flags,g.debug,g.event,g.entryerror,g.entrylen,g.vector,g.blocking,g.activity};
 const uint32_t ids[]={0x4402,0x4404,0x4406,0x6400,0x681e,0x681c,0x6800,0x6802,0x6804,0x2806,0x0802,0x6808,0x4802,0x4816,0x0804,0x680a,0x4804,0x4818,0x6818,0x4812,0x6816,0x4810,0x080e,0x6814,0x480e,0x4822,0x6820,0x6822,0x4016,0x4018,0x401a,0x4408,0x4824,0x4826};
 for(unsigned i=0;i<34;i++)fields[ids[i]]=vals[i];
}
static bool valid(void){return lup_validate(&state,&g,sizeof mem,0,readmem);}
int main(void){
 setup();assert(valid());assert(lup_inject(&state,&session)&&state.pending&&state.count==1&&session.native.cr2==g.address&&writes==3);assert(fields[0x681e]==g.rip&&fields[0x681c]==g.rsp&&fields[0x6820]==g.flags);
 for(unsigned k=0;k<26;k++){setup();switch(k){
 case 0:g.error=4;break;case 1:g.error=21;break;case 2:g.address++;break;case 3:g.rip=g.address=UINT64_C(0x800000000000);break;
 case 4:put(0x2000,0x3003);break;case 5:put(0x2000,UINT64_C(0x8000000000003007));break;case 6:put(0x4000,0x1007);break;case 7:put(0x3010,0x87);break;
 case 8:put(0xd040,UINT64_C(0x8000000000008001));break;case 9:put(0xd050,UINT64_C(0x800000000000a001));break;
 case 10:put(0x50e0,UINT64_C(0x00008e010010a000));break;case 11:put(0x6010,UINT64_C(0x00af9a000000ffff));break;
 case 12:g.trar=0x89;break;case 13:g.trlimit=11;break;case 14:g.trbase++;break;case 15:g.event=1;break;case 16:g.vector=1;break;case 17:g.flags&=~0x10000;break;case 18:g.debug=1;break;case 19:g.blocking=1;break;case 20:g.activity=1;break;case 21:missing=8;break;case 22:missing=10;break;case 23:g.cr4|=1u<<17;break;case 24:state.count=LNP_MAX_FAULTS;break;case 25:state.history[0]=(struct lnp_tuple){g.rip,g.address,g.cr3,g.error};state.count=1;break;}assert(!valid());}
 for(unsigned i=1;i<=34;i++){setup();failread=i;assert(!lup_inject(&state,&session)&&state.failed&&!state.pending&&!state.count&&!writes);}
 for(unsigned i=1;i<=3;i++){setup();failwrite=i;assert(!lup_inject(&state,&session)&&state.failed&&!state.pending&&!state.count&&!session.native.cr2);assert(!lup_inject(&state,&session));}
 puts("strict user PF host permission/metadata/ownership/failure matrix PASS");
}
