#include "../linux_apic_mmio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[0x10000];static struct guest_apic a;static struct lams_state tx;
static struct lmm_memory memory;static struct lamm_exit exitinfo;static uint64_t g[15],ripwrite,rspwrite;
static unsigned writes,failwrite,notifies;static bool failnotify;
static void put(unsigned p,uint64_t v){for(unsigned i=0;i<8;i++)ram[p+i]=(uint8_t)(v>>(i*8));}
static bool readram(void*c,uint64_t p,void*out,size_t n){(void)c;assert(p<sizeof ram&&n<=sizeof ram-p);memcpy(out,ram+p,n);return true;}
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;assert(tx.busy);if(f==0x681e)ripwrite=v;else{assert(f==0x681c);rspwrite=v;}return ++writes!=failwrite;}
static bool notify(void*c,uint64_t domain,uint64_t sequence,uint8_t vector){(void)c;assert(domain==3&&sequence==1&&vector==0x44);notifies++;return !failnotify;}
static void setup(uint8_t op,uint8_t modrm,unsigned offset){memset(ram,0,sizeof ram);memset(&tx,0,sizeof tx);memset(g,0,sizeof g);writes=failwrite=notifies=0;failnotify=false;ripwrite=rspwrite=UINT64_MAX;
 guest_apic_init_mode(&a,0,false);assert(lams_init(&tx,&a,3,0,notify));
 memory=(struct lmm_memory){.read=readram,.ram_size=sizeof ram,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2020,.efer=0xd01,.cs_ar=0xa09b,.cs_selector=0x10};
 put(0x1000,0x2023);put(0x2000,0x3023);put(0x3000,0x4023);put(0x4000,0x8063);put(0x4008,0xfee00063);
 ram[0x8000]=op;ram[0x8001]=modrm;g[1]=0x1000+offset;
 exitinfo=(struct lamm_exit){.rip=0,.rsp=0x12345678,.reason=48,.qualification=op==0x89?0x182:0x181,.linear=0x1000+offset,.physical=0xfee00000+offset};
}
static enum lams_result run(void){return lamm_apply(&tx,g,&memory,&exitinfo,0,1u<<9,true,0,wr);}
int main(void){
 const unsigned mapping[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 for(unsigned reg=0;reg<16;reg++){
  setup(0x8b,(uint8_t)(3|((reg&7)<<3)),0x30);
  if(reg>=8){ram[0x8002]=ram[0x8001];ram[0x8001]=0x8b;ram[0x8000]=0x44;}
  uint64_t before[15];memcpy(before,g,sizeof g);assert(run()==LAMS_COMMITTED);
  assert(ripwrite==(reg>=8?3:2));
  if(reg==4)assert(rspwrite==0x60014&&!memcmp(before,g,sizeof g));
  else{assert(g[mapping[reg]]==0x60014);before[mapping[reg]]=0x60014;assert(!memcmp(before,g,sizeof g));}
 }

 setup(0x8b,3,0x30);g[0]=UINT64_MAX;assert(run()==LAMS_COMMITTED&&g[0]==0x60014&&ripwrite==2&&writes==1);
 setup(0x8b,0x0b,0x30);assert(run()==LAMS_COMMITTED&&g[2]==0x60014); /* ECX is native[2]. */
 setup(0x8b,0x13,0x30);assert(run()==LAMS_COMMITTED&&g[3]==0x60014); /* EDX is native[3]. */
 setup(0x8b,0x23,0x30);assert(run()==LAMS_COMMITTED&&rspwrite==0x60014&&ripwrite==2&&writes==2);
 for(unsigned n=1;n<=2;n++){setup(0x8b,0x23,0x30);failwrite=n;struct guest_apic old=a;uint64_t before[15];memcpy(before,g,sizeof g);assert(run()==LAMS_FATAL&&tx.failed&&!memcmp(&old,&a,sizeof a)&&!memcmp(before,g,sizeof g));}
 setup(0x89,3,0xf0);g[0]=0x1ff;assert(run()==LAMS_COMMITTED&&a.irq.svr==0x1ff);
 setup(0x89,3,0xf0);g[0]=0x1ff;failwrite=1;struct guest_apic old=a;assert(run()==LAMS_FATAL&&!memcmp(&a,&old,sizeof a));
 setup(0x89,3,0xb0);uint32_t value=0x1ff;struct guest_apic_effect effect;assert(guest_apic_mmio(&a,0xf0,4,true,&value,0,&effect));assert(guest_apic_enqueue(&a,0x44,true)&&guest_apic_accept(&a,0x44));
 failnotify=true;assert(run()==LAMS_EFFECT_PENDING&&tx.pending&&notifies==1&&writes==1);old=a;assert(run()==LAMS_REJECTED);failnotify=false;assert(lams_retry(&tx)==LAMS_COMMITTED&&notifies==2&&!memcmp(&a,&old,sizeof a)&&writes==1);
 setup(0x8b,3,0x30);exitinfo.physical++;assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8b,3,0x30);exitinfo.linear++;assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8b,3,0x30);exitinfo.qualification=0x182;assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8b,3,0x30);exitinfo.ss_base=1;assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8b,3,0x30);a.x2apic=true;assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8b,3,0x30);put(0x4000,0x8000000000008063ull);assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8a,3,0x30);assert(run()==LAMS_REJECTED&&!writes); /* Width not device-supported. */
 setup(0x8b,3,0x30);exitinfo.rip=4095;ram[0x8fff]=0x8b;assert(run()==LAMS_REJECTED&&!writes); /* Never decode unread byte. */
 setup(0x8b,3,0x30);put(0x4008,0xfec00063);exitinfo.physical=0xfec00030;assert(run()==LAMS_REJECTED&&!writes);
 setup(0x8b,3,0x30);assert(lamm_apply(&tx,g,&memory,&exitinfo,0,0,true,0,wr)==LAMS_REJECTED&&!writes);
 puts("linux_apic_mmio: real decode/walk/model transactions, native GPR/RSP and failures PASS (host only)");
}
