#include "../linux_nonpresent_pf.h"
#include "linux_guest_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[0x1800000];static uint64_t fields[0x7000];static unsigned writes,failwrite;
static struct lgs_session s;static struct lmm_memory m;static struct lnp_exit e;static struct lnp_state state;
static void put(unsigned at,uint64_t value){for(unsigned i=0;i<8;i++)ram[at+i]=(uint8_t)(value>>(i*8));}
static bool read_ram(void*p,uint64_t a,void*b,size_t n){(void)p;if(a>=sizeof ram||n>sizeof ram-a)return false;memcpy(b,ram+a,n);return true;}
static void*map(void*p,uint64_t a){(void)p;return a>=0x100000&&a<0x100000+sizeof ram?ram+a-0x100000:0;}
static bool rd(void*p,uint32_t f,uint64_t*v){(void)p;*v=fields[f];return true;}
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;writes++;if(writes==failwrite)return false;fields[f]=v;return true;}
static void setup(void){memset(ram,0,sizeof ram);memset(fields,0,sizeof fields);memset(&s,0,sizeof s);state=(struct lnp_state){0};writes=failwrite=0;
 put(0x1000,0x2023);put(0x2000,0x3023);put(0x3000,0x4023);for(unsigned i=0;i<32;i++)put(0x4000+i*8,(uint64_t)i*4096|0x63);
 put(0xa000+224,0x00008e0000109000ULL);put(0xa000+232,0);put(0xd010,0x00af9b000000ffffULL);
 m=(struct lmm_memory){.read=read_ram,.ram_size=sizeof ram,.cr0=0x80050033,.cr3=0x1000,.cr4=0x20b0,.efer=0xd01,.cs_selector=0x10,.cs_ar=0xa09b};
 e=(struct lnp_exit){.info=0x80000b0e,.address=0x40000000,.rip=0x8000,.rsp=0xc010,.idtr=0xa000,.idt_limit=0xfff,.gdtr=0xd000,.gdt_limit=0x7f};
 s.prepared=s.memory.prepared=s.state.ready=s.state.guest_valid=true;s.memory.owner.map=map;s.platform.read_field=rd;s.hop.vmwrite=wr;
 for(unsigned i=0;i<sizeof ram/4096;i++)s.memory.pages[i]=0x100000+(uint64_t)i*4096;
 fields[0x6800]=m.cr0;fields[0x6802]=m.cr3;fields[0x6804]=m.cr4;fields[0x2806]=m.efer;fields[0x0802]=m.cs_selector;fields[0x4816]=m.cs_ar;
 fields[0x4404]=e.info;fields[0x6400]=e.address;fields[0x681e]=e.rip;fields[0x681c]=e.rsp;fields[0x6818]=e.idtr;fields[0x4812]=e.idt_limit;fields[0x6816]=e.gdtr;fields[0x4810]=e.gdt_limit;
}
int main(void){setup();assert(lnp_validate(&state,&m,&e));e.error=2;assert(lnp_validate(&state,&m,&e));
 for(unsigned bit=0;bit<32;bit++){if(bit==1)continue;setup();e.error=1ull<<bit;assert(!lnp_validate(&state,&m,&e));}
 for(unsigned bad=0;bad<15;bad++){setup();
 switch(bad){case 0:e.vectoring=0x80000b0e;break;case 1:e.event=0x80000044;break;case 2:e.blocking=1;break;case 3:e.info|=1u<<12;break;
 case 4:put(0x2008,0x3083);break;case 5:put(0x2008,0x3001);break;case 6:put(0x1000,0x100002023ULL);break;
 case 7:put(0xa000+224,0x00008e0100109000ULL);break;case 8:put(0xa000+224,0x00008e0000189000ULL);break;
 case 9:put(0xd010,0x00cf9b000000ffffULL);break;case 10:put(0x4000+9*8,0x8000000000009063ULL);break;
 case 11:put(0x4000+11*8,0xb061);break;case 12:put(0x4000+12*8,0xc061);break;case 13:e.rsp=32;break;case 14:m.cr4|=1u<<17;break;}
 assert(!lnp_validate(&state,&m,&e));}
 setup();e.error=2;put(0x1000,0x2021);assert(!lnp_validate(&state,&m,&e));
 setup();state.count=LNP_MAX_FAULTS;assert(!lnp_validate(&state,&m,&e));
 setup();assert(lnp_inject(&state,&s)&&state.pending&&state.count==1&&s.native.cr2==e.address&&fields[0x681e]==e.rip&&writes==3);
 struct lnp_state next;assert(!lnp_observe(&state,10,0x80000b0e,0,&next));assert(!lnp_observe(&state,52,0,0,&next));assert(!lnp_observe(&state,1,0,0xb0e,&next));
 assert(!lnp_observe(&state,0,0,0,&next));assert(!lnp_observe(&state,99,0,0,&next));assert(!lnp_observe(&state,0x80000021,0,0,&next));
 assert(!lnp_observe(&state,10,0,0x80000044,&next));assert(lnp_observe(&state,10,0,0x80000b0e,&next));assert(lnp_observe(&state,10,0,0xb0e,&next));
 assert(!lnp_validate(&next,&m,&e));m.cr3=0x5000;memcpy(ram+0x5000,ram+0x1000,4096);assert(lnp_validate(&next,&m,&e));
 for(unsigned i=1;i<=3;i++){setup();failwrite=i;assert(!lnp_inject(&state,&s)&&state.failed&&!state.pending&&!state.count&&!s.native.cr2);failwrite=0;assert(!lnp_inject(&state,&s));}
 setup();fields[0x4406]=2;assert(lnp_inject(&state,&s)&&fields[0x4018]==2&&fields[0x401a]==0&&fields[0x4016]==0x80000b0e);
 setup();m.cr3=0x16f5000;
 put(0x16f5888,0x16f6063);put(0x16f6000,0x16f7063);put(0x16f71f8,0);
 put(0x16f5ff8,0x162f067);put(0x162fff0,0x1630063);put(0x1630040,0x10001e3);put(0x1630058,0x16001e3);
 put(0x174a0e0,0x816c8e000010f3deULL);put(0x174a0e8,0xffffffff);put(0x1611010,0x00af9b000000ffffULL);
 e.address=0xffff888007ebc000ULL;e.rip=0xffffffff8115f733ULL;e.rsp=0xffffffff81603dc0ULL;
 e.idtr=0xffffffff8174a000ULL;e.gdtr=0xffffffff81611000ULL;
 assert(lnp_validate(&state,&m,&e)); /* Exact observed page-table/gate fixture, not a whitelist. */
 e.rip++;assert(lnp_validate(&state,&m,&e));e.address+=4096;assert(lnp_validate(&state,&m,&e));
 puts("linux_nonpresent_pf: strict error0/2, owned absence/permissions/gate/frame, repeat/budget/partial injection PASS (host only)");return 0;}
