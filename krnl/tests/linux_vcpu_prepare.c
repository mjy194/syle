#include "../linux_vcpu_prepare.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct linux_guest_memory m;
static struct lvp_caps c;
static struct lvp_plan p,before;
static uint64_t val(unsigned enc){
 for(unsigned i=0;i<p.count;i++)if(p.fields[i].encoding==enc)return p.fields[i].value;
 assert(!"missing encoding");return 0;
}
static void reject(void){
 memset(&p,0xa5,sizeof p);before=p;
 assert(!lvp_prepare(&m,&c,&p));assert(!memcmp(&p,&before,sizeof p));
}
int main(void){
 m.prepared=true;m.plan.ram_size=LGM_RAM_SIZE;m.plan.kernel_addr=0x1000000;
 m.plan.kernel_envelope=0xa00000;m.plan.entry_addr=0x1000200;m.plan.params_addr=0x90000;
 m.entry=(struct lb_entry_state){0x1000200,0x90000,0x80000,0x80000033,0x1000,0x620,0x500,0x4000,47,0x10,0x18};
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)m.pages[i]=0x10000000ull+4096ull*i;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)m.tables[i]=0x30000000ull+4096ull*i;
 m.eptp=m.tables[0]|0x1e;
 c=(struct lvp_caps){.pin=0xffffffff00000000ull,.primary=0xffffffff00000000ull,
 .secondary=0xffffffff00000000ull,.entry=0xffffffff00000000ull,.exit=0xffffffff00000000ull,
 .ept=(1ull<<6)|(1ull<<14)|(1ull<<20)|(1ull<<25),
 .cr0_fixed0=0x21,.cr0_fixed1=0xffffffff,.cr4_fixed0=0x2000,.cr4_fixed1=0xffffffff,
 .physical_bits=36,.timer_shift=5,.want_wbinvd=true};
 assert(lvp_prepare(&m,&c,&p));assert(p.count<=LVP_MAX_FIELDS);
 for(unsigned i=0;i<p.count;i++){for(unsigned j=0;j<i;j++)assert(p.fields[i].encoding!=p.fields[j].encoding);
 /* VMCS host field categories forbidden. */
 assert((p.fields[i].encoding&0xc00)!=0xc00);}
 assert(val(0x201a)==m.eptp);assert(val(0x4004)==0xffffffff);assert(val(0x4000)==0x49);
 assert(val(0x401e)==0x42);assert(val(0x80e)==0x20&&val(0x6814)==0x6000&&val(0x480e)==103&&val(0x4822)==0x8b);assert(!(val(0x4002)&((1u<<25)|(1u<<28)|(1u<<21))));
 assert(val(0x4002)&(1u<<24));assert(val(0x6804)==0x2620);assert(val(0x6006)==0x620);
 assert(val(0x681e)==m.entry.rip&&val(0x6802)==0x1000&&val(0x2806)==0x500);
 assert(val(0x482e)==(1u<<23));assert(p.gpr[4]==0x90000);
 for(unsigned i=0;i<15;i++)if(i!=4)assert(!p.gpr[i]);
 c.secondary&=~(1ull<<38);reject();c.want_wbinvd=false;reject();c.want_wbinvd=true;
 c.secondary|=1ull<<5;reject();c.secondary=0xffffffff00000000ull;
 c.primary|=1ull<<25;reject();c.primary=0xffffffff00000000ull;
 c.pin|=1ull<<5;reject();c.pin=0xffffffff00000000ull;
 c.entry|=1ull<<10;reject();c.entry=0xffffffff00000000ull;
 c.exit|=1ull<<15;reject();c.exit=0xffffffff00000000ull;
 c.primary&=~(1ull<<56);reject();c.primary=0xffffffff00000000ull;
 c.ept&=~(1ull<<25);reject();c.ept|=1ull<<25;
 c.timer_shift=21;reject();c.timer_shift=20;assert(lvp_prepare(&m,&c,&p));assert(val(0x482e)==256);
 c.physical_bits=53;reject();c.physical_bits=31;reject();c.physical_bits=36;
 uint64_t saved=m.pages[LGM_PAGE_COUNT-1];m.pages[LGM_PAGE_COUNT-1]=1ull<<36;reject();
 m.pages[LGM_PAGE_COUNT-1]=saved+1;reject();m.pages[LGM_PAGE_COUNT-1]=saved;
 saved=m.tables[LGM_TABLE_COUNT-1];m.tables[LGM_TABLE_COUNT-1]=1ull<<36;reject();m.tables[LGM_TABLE_COUNT-1]=saved;
 m.eptp^=0x40;reject();m.eptp^=0x40;
 c.cr4_fixed1&=~0x2000ull;reject();c.cr4_fixed1=0xffffffff;
 c.cr0_fixed0|=0x10000;reject();c.cr0_fixed0=0x21;
 m.entry.gdt_limit=31;reject();m.entry.gdt_limit=47;
 m.entry.efer|=0x800;reject();m.entry.efer=0x500;
 m.entry.rip++;reject();m.entry.rip--;
 m.plan.kernel_envelope=LGM_RAM_SIZE;reject();m.plan.kernel_envelope=0xa00000;
 m.prepared=false;reject();m.prepared=true;
 assert(!lvp_prepare(0,&c,&p)&&!lvp_prepare(&m,0,&p)&&!lvp_prepare(&m,&c,0));
 assert(lvp_prepare(&m,&c,&p));
 puts("linux_vcpu_prepare: PASS (pure control/guest plan, no VM)");
}
