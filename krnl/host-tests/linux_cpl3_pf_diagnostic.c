#include "linux_pf_diagnostic.h"
#include "linux_guest_session.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct lgs_session s;static unsigned char mem[16][4096];static uint64_t fields[0x7000];static unsigned reads,maps;static uint32_t failfield;static bool mapfail;
static void*map(void*c,uint64_t pa){(void)c;maps++;return !mapfail&&pa>=4096&&pa<=sizeof mem?mem[pa/4096-1]:0;}
static bool rd(void*c,uint32_t f,uint64_t*v){(void)c;reads++;if(f==failfield)return false;*v=fields[f];return true;}
static void put(uint64_t pa,uint64_t v){for(unsigned i=0;i<8;i++)mem[(pa+i)/4096-1][(pa+i)%4096]=v>>(8*i);}
static void setup(void){
 memset(&s,0,sizeof s);memset(mem,0,sizeof mem);memset(fields,0,sizeof fields);maps=reads=0;failfield=0;mapfail=false;
 s.memory.prepared=s.host.active=s.host.ready=true;s.memory.owner.map=map;s.platform.read_field=rd;
 for(unsigned i=1;i<=16;i++)s.memory.pages[i]=i*4096;
 put(0x1000,0x2003);put(0x2000,0x3003);put(0x3000,0x4003);
 for(unsigned i=1;i<=15;i++)put(0x4000+i*8,i*4096+3);
 fields[0x4404]=0x80000b0e;fields[0x6802]=0x1000;fields[0x6400]=fields[0x681e]=0x5000;
 fields[0x6818]=0x6000;fields[0x4812]=4095;fields[0x6816]=0x7000;fields[0x4810]=127;
 fields[0x0802]=0x33;fields[0x4816]=0xa0fb;fields[0x080e]=0x40;fields[0x6814]=0x8000;fields[0x480e]=103;fields[0x4822]=0x8b;
 fields[0x0804]=0x2b;fields[0x4818]=0xc0f3;fields[0x6820]=0x10202;
 put(0x60e0,UINT64_C(0x00008e0000109000));put(0x60e8,0);
 put(0x7010,UINT64_C(0x00af9b000000ffff));put(0x7030,UINT64_C(0x00affb000000ffff));
 put(0x7040,UINT64_C(0x00008b0080000067));put(0x7048,0);put(0x8004,0xb010);put(0x8024,0xc010);
}
int main(void){struct lgpd_snapshot d;setup();assert(lgpd_collect(&s,&d)&&d.valid==((1ull<<LGPD_FIELDS)-1)&&reads==LGPD_FIELDS&&maps<5000);
 assert((d.data_valid&240)==240&&d.target_csdesc==UINT64_C(0x00af9b000000ffff)&&d.csdesc==UINT64_C(0x00affb000000ffff));
 assert(d.tss_rsp0==0xb010&&d.selected_stack==0xb010&&d.selected_slot==0x8004&&d.va[10]==0xafe0&&d.va[11]==0xb00f);
 assert(d.trace[10].status==1&&d.trace[11].status==1);
 put(0x60e0,UINT64_C(0x00008e0100109000));assert(lgpd_collect(&s,&d)&&d.selected_stack==0xc010&&d.selected_slot==0x8024);
 static const uint32_t added[]={0x6808,0x4802,0x0804,0x680a,0x4804,0x4818,0x080e,0x6814,0x480e,0x4822,0x6820,0x6822,0x4016,0x4018,0x401a,0x4408,0x4824,0x4826};
 for(unsigned i=0;i<sizeof added/sizeof added[0];i++){setup();failfield=added[i];assert(lgpd_collect(&s,&d)&&!(d.valid&(1ull<<(16+i)))&&!d.data_valid&&!maps);}
 setup();mapfail=true;assert(lgpd_collect(&s,&d)&&!d.data_valid);
 setup();s.memory.pages[8]=0;assert(lgpd_collect(&s,&d)&&!(d.data_valid&192));
 setup();s.memory.pages[8]=UINT64_C(1)<<52;assert(lgpd_collect(&s,&d)&&!(d.data_valid&192));
 setup();fields[0x480e]=10;assert(lgpd_collect(&s,&d)&&!(d.data_valid&192));
 setup();fields[0x6814]=UINT64_MAX-3;assert(lgpd_collect(&s,&d)&&!(d.data_valid&192));
 setup();put(0x8004,0);assert(lgpd_collect(&s,&d)&&(d.data_valid&128)&&!d.trace[10].count);
 setup();s.state.host_dirty=true;reads=0;assert(!lgpd_collect(&s,&d)&&!reads);
 setup();struct lgs_session old=s;unsigned char saved[sizeof mem];memcpy(saved,mem,sizeof mem);assert(lgpd_collect(&s,&d)&&!memcmp(&s,&old,sizeof s)&&!memcmp(saved,mem,sizeof mem));
 puts("CPL3 diagnostic: raw cached fields/targetCS/TR/TSS/IST/bounded traces/private read failures PASS");
}
