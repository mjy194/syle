#include "../linux_ioapic_mmio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char ram[65536];static struct guest_apic a;static struct ioapic io;static struct gia bridge;static struct lio_state tx;static struct lmm_memory mem;static struct lamm_exit ex;static uint64_t g[15];static unsigned writes,fail;static uint64_t rsp,rip;
static struct guest_apic original_a;static struct ioapic original_io;
static void put(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)ram[p+i]=v>>(i*8);}
static bool rd(void*c,uint64_t p,void*b,size_t n){(void)c;assert(p<sizeof ram&&n<=sizeof ram-p);memcpy(b,ram+p,n);return true;}
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;assert(tx.busy);/* Callback proves no staged enqueue leaked into live domain. */assert(!memcmp(&a,&original_a,sizeof a)&&!memcmp(&io,&original_io,sizeof io));if(f==0x681c)rsp=v;else{assert(f==0x681e);rip=v;}return ++writes!=fail;}
static void setup(unsigned op,unsigned modrm,unsigned off){memset(&tx,0,sizeof tx);memset(ram,0,sizeof ram);memset(g,0,sizeof g);writes=fail=0;rsp=rip=0;guest_apic_init(&a,0);struct guest_apic_effect e;assert(guest_apic_write(&a,0x80f,0x1ff,0,&e));ioapic_init_guest(&io,gia_enqueue,&bridge);assert(gia_init(&bridge,&a,&io,15,true));io.pins[4].low=0x18028;io.pins[4].asserted=true;io.selector=0x18;assert(lio_init(&tx,&bridge));mem=(struct lmm_memory){.read=rd,.ram_size=sizeof ram,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2020,.efer=0xd01,.cs_ar=0xa09b,.cs_selector=0x10};put(0x1000,0x2023);put(0x2000,0x3023);put(0x3000,0x4023);put(0x4000,0x8063);put(0x4008,0xfec00063);ram[0x8000]=op;ram[0x8001]=modrm;g[1]=0x1000+off;ex=(struct lamm_exit){.rip=0,.rsp=0x12345678,.reason=48,.qualification=op==0x89?0x182:0x181,.linear=0x1000+off,.physical=0xfec00000+off};}
static enum lams_result run(void){original_a=a;original_io=io;return lio_apply(&tx,g,&mem,&ex,1u<<9,true,0,wr);}
int main(void){setup(0x89,3,0x10);g[0]=0x8028;assert(run()==LAMS_COMMITTED&&writes==1&&rip==2);assert(io.pins[4].remote_irr&&(a.irq.irr[1]&(1u<<8))&&(a.pending_level[1]&(1u<<8)));assert(io.context==&bridge&&io.enqueue==gia_enqueue&&bridge.apic==&a&&bridge.io==&io);
 setup(0x89,3,0x10);g[0]=0x8028;fail=1;struct gia oldbridge=bridge;uint64_t oldg[15];memcpy(oldg,g,sizeof g);assert(run()==LAMS_FATAL&&tx.failed&&!memcmp(&original_a,&a,sizeof a)&&!memcmp(&original_io,&io,sizeof io)&&!memcmp(&oldbridge,&bridge,sizeof bridge)&&!memcmp(oldg,g,sizeof g));unsigned n=writes;assert(run()==LAMS_REJECTED&&writes==n);
 for(unsigned f=0;f<=2;f++){setup(0x8b,0x23,0x10);io.selector=1;fail=f;enum lams_result rc=run();assert(rc==(f?LAMS_FATAL:LAMS_COMMITTED));if(!f)assert(rsp==0x170011&&rip==2&&writes==2);else assert(!memcmp(&original_a,&a,sizeof a)&&!memcmp(&original_io,&io,sizeof io));}
 setup(0x89,3,0);g[0]=1;assert(run()==LAMS_COMMITTED&&io.selector==1);
 setup(0x89,3,0x10);g[0]=0x8028;a.irq.svr=0xff;assert(run()==LAMS_REJECTED&&!writes&&!memcmp(&original_io,&io,sizeof io)&&!memcmp(&original_a,&a,sizeof a));
 for(unsigned mode=0;mode<6;mode++){setup(0x8b,3,0x10);if(mode==0)bridge.reserved=true;if(mode==1)bridge.quarantined=true;if(mode==2)bridge.receipt_pending=true;if(mode==3)ex.physical++;if(mode==4)ex.vectoring=0x80000020;if(mode==5)ram[0x8000]=0x8a;assert(run()==LAMS_REJECTED&&!writes);}
 puts("IOAPIC full-domain staged MMIO host tests PASS");}
