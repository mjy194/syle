#include "../linux_apic_dispatch.h"
#include "linux_guest_session.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct lgs_session session;static struct guest_apic apic;static struct ioapic io;static struct gia irq;static struct lams_state tx;static struct lad_config cfg;static struct lio_state lio;
static uint64_t f[0x7000];static uint8_t ram[0x10000];static unsigned writes;static bool clockfail;
static bool rd(void*c,uint32_t field,uint64_t*out){(void)c;*out=f[field];return true;}
static bool wr(void*c,uint32_t field,uint64_t v){(void)c;writes++;f[field]=v;return true;}
static bool now(void*c,uint64_t*out){(void)c;*out=0;return !clockfail;}
static void*map(void*c,uint64_t p){(void)c;return p>=0x100000&&p<0x110000?ram+p-0x100000:NULL;}
static void put(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)ram[p+i]=(uint8_t)(v>>(8*i));}
static void setup(void){memset(&session,0,sizeof session);memset(&tx,0,sizeof tx);memset(&lio,0,sizeof lio);memset(f,0,sizeof f);memset(ram,0,sizeof ram);writes=0;clockfail=false;
 guest_apic_init_mode(&apic,0,false);ioapic_init(&io,gia_enqueue,&irq);assert(gia_init(&irq,&apic,&io,91,true));assert(lams_init(&tx,&apic,91,&irq,gia_notify));assert(lio_init(&lio,&irq));cfg=(struct lad_config){.transactions=&tx,.irq=&irq,.now=now,.ioapic=&lio};
 session.prepared=session.memory.prepared=session.state.ready=session.state.guest_valid=true;session.memory.owner.map=map;for(unsigned i=0;i<16;i++)session.memory.pages[i]=0x100000+i*4096;
 session.platform.read_field=rd;session.hop.vmwrite=wr;f[0x0802]=0x10;f[0x4816]=0xa09b;f[0x6800]=0x80010033;f[0x6802]=0x1000;f[0x6804]=0x2020;f[0x2806]=0xd01;
 f[0x4402]=48;f[0x6400]=0x181;f[0x640a]=0x1030;f[0x2400]=0xfee00030;f[0x681c]=0x8000;session.native.gpr[1]=0x1030;
 put(0x1000,0x2023);put(0x2000,0x3023);put(0x3000,0x4023);put(0x4000,0x8063);put(0x4008,0xfee00063);ram[0x8000]=0x8b;ram[0x8001]=3;
}
int main(void){setup();assert(lad_valid(&cfg)&&lad_mmio(&cfg,&session,1u<<9)==LAMS_COMMITTED&&session.native.gpr[0]==0x60014&&writes==1);
 const unsigned fields[]={0x6806,0x6808,0x680a,0x680c};for(unsigned i=0;i<4;i++){setup();f[fields[i]]=1;assert(lad_mmio(&cfg,&session,1u<<9)==LAMS_REJECTED&&!writes);}
 setup();session.memory.pages[8]=0;assert(lad_mmio(&cfg,&session,1u<<9)==LAMS_REJECTED&&!writes);
 setup();clockfail=true;assert(lad_mmio(&cfg,&session,1u<<9)==LAMS_REJECTED&&!writes);
 setup();irq.reserved=true;assert(lad_mmio(&cfg,&session,1u<<9)==LAMS_REJECTED&&!writes);
 setup();f[0x4016]=0x80000044;assert(lad_mmio(&cfg,&session,1u<<9)==LAMS_REJECTED&&!writes);
 setup();tx.domain++;assert(!lad_valid(&cfg)&&lad_mmio(&cfg,&session,1u<<9)==LAMS_REJECTED&&!writes);
 setup();put(0x4008,0xfec00063);f[0x640a]=0x1000;f[0x2400]=0xfec00000;session.native.gpr[1]=0x1000;assert(lad_mmio(&cfg,&session,1u<<9)==LAMS_COMMITTED&&writes==1);
 setup();f[0x4402]=31;f[0x440c]=2;session.native.gpr[2]=0x1b;assert(lad_msr(&cfg,&session,1u<<21,1u<<9)==LAMS_COMMITTED&&session.native.gpr[0]==0xfee00900);
 setup();f[0x4402]=31;f[0x440c]=2;session.native.gpr[2]=0x803;assert(lad_msr(&cfg,&session,1u<<21,1u<<9)==LAMS_REJECTED&&!writes);
 puts("linux_apic_dispatch: real VMCS fields/owned RAM/domain binding and MSR/MMIO routing PASS (host only)");
}
