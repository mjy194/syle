#include "linux_user_pf.h"
#include "linux_guest_session.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char mem[65536];static struct lgs_session user_session;static struct lup_snapshot user_g;static struct lnp_state user_state;static uint64_t user_fields[0x7000];static unsigned reads,writes,failread,failwrite;static uint64_t missing;
#define K UINT64_C(0xffff800000000000)
static void user_put(uint64_t a,uint64_t v){assert(a<=sizeof mem-8);for(unsigned i=0;i<8;i++)mem[a+i]=v>>(8*i);}
static bool readmem(void*c,uint64_t a,void*b,size_t n){(void)c;if(a>=sizeof mem||n>sizeof mem-a||(missing&&a/4096==missing))return false;memcpy(b,mem+a,n);return true;}
static void*user_map(void*c,uint64_t p){(void)c;return p<sizeof mem&&p/4096!=missing?mem+p:0;}
static bool user_rd(void*c,uint32_t f,uint64_t*v){(void)c;if(++reads==failread)return false;*v=user_fields[f];return true;}
static bool user_wr(void*c,uint32_t f,uint64_t v){(void)c;user_fields[f]=v;return ++writes!=failwrite;}
static void user_setup(void){
 memset(mem,0,sizeof mem);memset(&user_session,0,sizeof user_session);memset(user_fields,0,sizeof user_fields);user_state=(struct lnp_state){0};missing=reads=writes=failread=failwrite=0;
 user_g=(struct lup_snapshot){.info=0x80000b0e,.error=20,.address=0x400000,.rip=0x400000,.rsp=0x7ffd42a72f40,.cr0=0x80050033,.cr3=0x1000,.cr4=0x26b0,.efer=0xd01,.cs=0x33,.cslimit=UINT32_MAX,.csar=0xa0fb,.ss=0x2b,.sslimit=UINT32_MAX,.ssar=0xc0f3,.idtr=K+0x5000,.idtlimit=4095,.gdtr=K+0x6000,.gdtlimit=127,.tr=0x40,.trbase=K+0x9000,.trlimit=103,.trar=0x8b,.flags=0x10202};
 user_put(0x1000,0x2007);user_put(0x2000,0x3007);user_put(0x3010,0x4007); /* A=0 accepted, final PTE absent */
 user_put(0x1800,0xb003);user_put(0xb000,0xc003);user_put(0xc000,0xd003);
 for(unsigned i=5;i<=10;i++)user_put(0xd000+i*8,(i*4096)|1|(i==8?2:0)|(i==10?0:UINT64_C(1)<<63));
 user_put(0x50e0,UINT64_C(0x00008e000010a000));user_put(0x50e8,0xffff8000);
 user_put(0x6010,UINT64_C(0x00af9b000000ffff));user_put(0x6040,UINT64_C(0x00008b0090000067));user_put(0x6048,0xffff8000);user_put(0x9004,K+0x9000);
 user_session.prepared=user_session.memory.prepared=user_session.host.active=user_session.host.ready=user_session.state.ready=user_session.state.guest_valid=true;
 user_session.memory.owner.map=user_map;user_session.platform.read_field=user_rd;user_session.hop.vmwrite=user_wr;
 for(unsigned i=1;i<16;i++)user_session.memory.pages[i]=i*4096;
 uint64_t vals[]={user_g.reason,user_g.info,user_g.error,user_g.address,user_g.rip,user_g.rsp,user_g.cr0,user_g.cr3,user_g.cr4,user_g.efer,user_g.cs,user_g.csbase,user_g.cslimit,user_g.csar,user_g.ss,user_g.ssbase,user_g.sslimit,user_g.ssar,user_g.idtr,user_g.idtlimit,user_g.gdtr,user_g.gdtlimit,user_g.tr,user_g.trbase,user_g.trlimit,user_g.trar,user_g.flags,user_g.debug,user_g.event,user_g.entryerror,user_g.entrylen,user_g.vector,user_g.blocking,user_g.activity};
 const uint32_t ids[]={0x4402,0x4404,0x4406,0x6400,0x681e,0x681c,0x6800,0x6802,0x6804,0x2806,0x0802,0x6808,0x4802,0x4816,0x0804,0x680a,0x4804,0x4818,0x6818,0x4812,0x6816,0x4810,0x080e,0x6814,0x480e,0x4822,0x6820,0x6822,0x4016,0x4018,0x401a,0x4408,0x4824,0x4826};
 for(unsigned i=0;i<34;i++)user_fields[ids[i]]=vals[i];
}
static bool valid(void){return lup_validate(&user_state,&user_g,sizeof mem,0,readmem);}
static void write_setup(void){user_setup();user_g.error=6;user_g.address=0x401050;user_put(0x4000,0xe005);user_fields[0x4406]=6;user_fields[0x6400]=user_g.address;}
int user_model_tests(void){
 user_setup();assert(valid());assert(lup_inject(&user_state,&user_session)&&user_state.pending&&user_state.count==1&&user_session.native.cr2==user_g.address&&writes==3);assert(user_fields[0x681e]==user_g.rip&&user_fields[0x681c]==user_g.rsp&&user_fields[0x6820]==user_g.flags);
 for(unsigned k=0;k<26;k++){user_setup();switch(k){
 case 0:user_g.error=4;break;case 1:user_g.error=21;break;case 2:user_g.address++;break;case 3:user_g.rip=user_g.address=UINT64_C(0x800000000000);break;
 case 4:user_put(0x2000,0x3003);break;case 5:user_put(0x2000,UINT64_C(0x8000000000003007));break;case 6:user_put(0x4000,0x1007);break;case 7:user_put(0x3010,0x87);break;
 case 8:user_put(0xd040,UINT64_C(0x8000000000008001));break;case 9:user_put(0xd050,UINT64_C(0x800000000000a001));break;
 case 10:user_put(0x50e0,UINT64_C(0x00008e010010a000));break;case 11:user_put(0x6010,UINT64_C(0x00af9a000000ffff));break;
 case 12:user_g.trar=0x89;break;case 13:user_g.trlimit=11;break;case 14:user_g.trbase++;break;case 15:user_g.event=1;break;case 16:user_g.vector=1;break;case 17:user_g.flags&=~0x10000;break;case 18:user_g.debug=1;break;case 19:user_g.blocking=1;break;case 20:user_g.activity=1;break;case 21:missing=8;break;case 22:missing=10;break;case 23:user_g.cr4|=1u<<17;break;case 24:user_state.count=LNP_MAX_FAULTS;break;case 25:user_state.history[0]=(struct lnp_tuple){user_g.rip,user_g.address,user_g.cr3,user_g.error};user_state.count=1;break;}assert(!valid());}
 for(unsigned i=1;i<=34;i++){user_setup();failread=i;assert(!lup_inject(&user_state,&user_session)&&user_state.failed&&!user_state.pending&&!user_state.count&&!writes);}
 for(unsigned i=1;i<=3;i++){user_setup();failwrite=i;assert(!lup_inject(&user_state,&user_session)&&user_state.failed&&!user_state.pending&&!user_state.count&&!user_session.native.cr2);assert(!lup_inject(&user_state,&user_session));}

 write_setup();assert(valid());assert(lup_inject(&user_state,&user_session)&&user_state.history[0].error==6&&user_state.pending&&user_fields[0x4018]==6&&user_session.native.cr2==0x401050);
 for(unsigned k=0;k<12;k++){write_setup();switch(k){
 case 0:user_put(0x3010,0x4005);break;case 1:user_put(0x3010,0x4003);break;case 2:user_put(0x4000,0xe001);break;
 case 3:user_put(0x4000,UINT64_C(0x800000000000e005));break;case 4:user_put(0x4000,0);break;case 5:missing=14;break;
 case 6:user_put(0x4008,0xe007);break;case 7:user_g.error=7;break;case 8:user_g.error=5;break;case 9:user_g.address=UINT64_C(0x800000000000);break;
 case 10:user_put(0x3010,UINT64_C(0x8000000000004007));break;case 11:user_g.rip=0x401000;break;
 }assert(!valid());}
 for(unsigned i=1;i<=34;i++){write_setup();failread=i;assert(!lup_inject(&user_state,&user_session)&&user_state.failed&&!writes&&!user_state.count);}
 /* NX data ancestor is permitted for writes; separate present execution path. */
 write_setup();user_g.address=0x600000;user_fields[0x6400]=user_g.address;user_put(0x3018,UINT64_C(0x800000000000f007));assert(valid());
 for(unsigned i=1;i<=3;i++){write_setup();failwrite=i;assert(!lup_inject(&user_state,&user_session)&&user_state.failed&&!user_state.count&&!user_state.pending&&!user_session.native.cr2);}
 /* User read fault: data ancestors may be RO/NX, but must remain user.
  * Instruction RIP independently requires an owned executable user mapping. */
 write_setup();user_g.error=4;user_fields[0x4406]=4;
 user_put(0x3010,0x4005);assert(valid());
 assert(lup_inject(&user_state,&user_session)&&user_fields[0x4018]==4&&user_state.history[0].error==4);
 write_setup();user_g.error=4;user_g.address=0x600000;user_fields[0x4406]=4;user_fields[0x6400]=user_g.address;
 user_put(0x3018,UINT64_C(0x800000000000f005));assert(valid());
 for(unsigned k=0;k<8;k++){
  write_setup();user_g.error=4;
  switch(k){case 0:user_put(0x3010,0x4003);break;
   case 1:user_put(0x4000,0xe001);break;case 2:user_put(0x4000,0);break;
   case 3:user_put(0x4008,0xe005);break;case 4:user_g.error=5;break;
   case 5:user_g.error=7;break;case 6:user_put(0x4000,UINT64_C(0x800000000000e005));break;
   case 7:user_g.address=UINT64_C(0x800000000000);break;}
  assert(!valid());
 }
 for(unsigned i=1;i<=34;i++){
  write_setup();user_g.error=4;user_fields[0x4406]=4;failread=i;
  assert(!lup_inject(&user_state,&user_session)&&user_state.failed&&!writes&&!user_state.count);
 }
 for(unsigned i=1;i<=3;i++){
  write_setup();user_g.error=4;user_fields[0x4406]=4;failwrite=i;
  assert(!lup_inject(&user_state,&user_session)&&user_state.failed&&!user_state.count&&!user_state.pending&&!user_session.native.cr2);
 }
 puts("strict user PF host permission/metadata/ownership/failure matrix PASS");return 0;
}
