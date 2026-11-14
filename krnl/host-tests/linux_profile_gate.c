#include "../linux_profile_gate.h"
#include "../linux_profile_activation.h"
#include "../linux_acpi_reduced.h"
#include "linux_guest_session.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static struct lgs_session s;static struct guest_apic apic;static struct ioapic io;static struct gia gia;static struct lams_state tx;static struct lio_state lio;static struct lad_config cfg;
static uint8_t*ram,*tables;static uint64_t f[0x7000];static bool readfail,clockfail;static uint64_t current_time;
static bool rd(void*c,uint32_t field,uint64_t*out){(void)c;if(readfail)return false;*out=f[field];return true;}
static bool wr(void*c,uint32_t field,uint64_t value){(void)c;(void)field;(void)value;assert(0);return false;}
static bool now(void*c,uint64_t*out){(void)c;*out=current_time;return !clockfail;}
static void*map(void*c,uint64_t p){(void)c;if(p>=0x100000&&p<0x100000+LGM_RAM_SIZE)return ram+p-0x100000;if(p>=0x20000000&&p<0x20000000+LGM_TABLE_COUNT*4096)return tables+p-0x20000000;return NULL;}
static void* map_alt(void*c,uint64_t p){return map(c,p);}
static bool rd_alt(void*c,uint32_t f,uint64_t*v){return rd(c,f,v);}
static bool wr_alt(void*c,uint32_t f,uint64_t v){return wr(c,f,v);}
static bool now_alt(void*c,uint64_t*v){return now(c,v);}
static void put(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));}
static void setup(void){memset(&s,0,sizeof s);memset(&tx,0,sizeof tx);memset(&lio,0,sizeof lio);memset(f,0,sizeof f);memset(ram,0,LGM_RAM_SIZE);memset(tables,0,LGM_TABLE_COUNT*4096);readfail=clockfail=false;current_time=1;
 guest_apic_init_mode(&apic,1,false);ioapic_init_guest(&io,gia_enqueue,&gia);assert(gia_init(&gia,&apic,&io,123,true));assert(lams_init(&tx,&apic,123,&gia,gia_notify));assert(lio_init(&lio,&gia));cfg=(struct lad_config){.transactions=&tx,.irq=&gia,.now=now,.ioapic=&lio};
 s.prepared=s.enabled=s.host.active=s.host.ready=s.memory.prepared=s.state.ready=s.state.guest_valid=true;s.host.caps.physical_bits=32;s.host.caps.timer_shift=5;
 s.memory.owner.map=map;s.platform.read_field=rd;s.hop.vmwrite=s.host.owner.vmwrite=wr;s.memory.plan.ram_size=LGM_RAM_SIZE;s.memory.plan.params_addr=0x90000;s.memory.entry.rip=0x1000200;s.memory.entry.rsp=0x80000;s.native.gpr[4]=0x90000; /* Native index4 is RSI, Linux boot_params. */
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)s.memory.pages[i]=0x100000+(uint64_t)i*4096;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)s.memory.tables[i]=0x20000000+(uint64_t)i*4096;
 s.memory.eptp=s.host.eptp=s.memory.tables[0]|0x1e;
 put(tables,s.memory.tables[1]|7,8);put(tables+4096,s.memory.tables[2]|7,8);
 for(unsigned i=0;i<LGM_PT_COUNT;i++){put(tables+8192+8*i,s.memory.tables[i+3]|7,8);for(unsigned j=0;j<512;j++)put(tables+(i+3)*4096+8*j,s.memory.pages[i*512+j]|0x37,8);}
 f[0x4000]=0x49;f[0x4002]=(1u<<7)|(1u<<9)|(1u<<10)|(1u<<11)|(1u<<12)|(1u<<15)|(1u<<16)|(1u<<19)|(1u<<20)|(1u<<23)|(1u<<24)|(1u<<29)|(1u<<30)|(1u<<31);
 f[0x401e]=0x42;f[0x4012]=(1u<<2)|(1u<<9)|(1u<<14)|(1u<<15);f[0x400c]=(1u<<2)|(1u<<9)|(15u<<18);f[0x4004]=LVP_EXCEPTION_BITMAP;f[0x6000]=f[0x6002]=UINT64_MAX;f[0x482e]=1u<<23;f[0x681e]=s.memory.entry.rip;f[0x681c]=s.memory.entry.rsp;f[0x201a]=s.memory.eptp;
 uint8_t*bp=ram+0x90000;put(bp+0x70,LAT_GPA,8);put(bp+0x202,0x53726448,4);put(bp+0x206,0x20f,2);bp[0x1e8]=1;put(bp+0x2d0,LAT_GPA,8);put(bp+0x2d8,LAT_REGION_SIZE,8);put(bp+0x2e0,2,4);size_t n=0;assert(lar_build(ram+LAT_GPA,LAR_SIZE,LAT_GPA,&n)&&n==LAR_SIZE);
}
static struct lvp_cpu_features host={UINT32_MAX,UINT32_MAX,32};
static void reject(void){struct lpg_choice choice={LCP_BASELINE,0xdead,0xbeef};assert(!lpg_prepare(&cfg,&s,&host,&choice)&&choice.kind==LCP_BASELINE&&choice.domain==0xdead&&choice.initial_time==0xbeef);}
int main(void){ram=calloc(1,LGM_RAM_SIZE);tables=calloc(LGM_TABLE_COUNT,4096);assert(ram&&tables);setup();struct lpg_choice choice={0};assert(lpg_prepare(&cfg,&s,&host,&choice)&&choice.kind==LCP_EXPERIMENTAL_APIC&&choice.domain==123&&choice.initial_time==1);
 setup();f[0x4000]|=0x16;f[0x4002]|=0x0401e172;f[0x4012]|=0x11ff;f[0x400c]|=0x36dff;assert(lpg_prepare(&cfg,&s,&host,&choice));
 const unsigned fields[]={0x4000,0x4002,0x401e,0x4012,0x400c,0x4004,0x6000,0x6002,0x482e,0x201a};for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++){setup();f[fields[i]]=0;reject();}
 const unsigned zero_fields[]={0x4006,0x4008,0x400a,0x400e,0x4010,0x4014,0x4016,0x4018,0x401a};for(unsigned i=0;i<9;i++){setup();f[zero_fields[i]]=1;reject();}
 setup();f[0x4002]|=1u<<28;reject();setup();f[0x4002]|=1u<<25;reject();setup();f[0x4002]|=1ull<<40;reject();
 setup();put(tables+8,s.memory.tables[1]|7,8);reject();setup();put(tables+4096+24,s.memory.tables[2]|7,8);reject();setup();tables[3*4096]^=4;reject();setup();s.memory.pages[0]=0;reject();setup();s.memory.tables[66]=0;reject();
 setup();ram[LAT_GPA+40]^=1;reject();setup();ram[0x90072]=0;reject();setup();ram[0x902e0]=1;reject();
 setup();s.exits=1;reject();setup();s.native.resume=1;reject();setup();s.state.host_dirty=true;reject();setup();s.host.quarantined=true;reject();setup();readfail=true;reject();setup();clockfail=true;reject();setup();current_time=0;reject();
 setup();s.host.caps.timer_shift=21;f[0x482e]=1u<<7;reject();
 setup();tx.domain++;reject();setup();gia.reserved=true;reject();setup();io.guest_profile=false;reject();setup();io.pins[4].low=0x8044;reject();setup();apic.x2apic=true;reject();setup();apic.timer.armed=true;reject();

 setup();struct lpa_state activation={0};assert(lpa_before(&activation,&cfg,&s,&host)&&activation.active&&activation.kind==LCP_EXPERIMENTAL_APIC);
 /* Mutable guest boot state/event ownership must not be rechecked as fresh. */
 ram[LAT_GPA]^=1;ram[0x90070]=0;s.exits=1;s.native.resume=1;s.native.gpr[4]=0;
 f[0x681e]=0xdead;f[0x681c]=0xbeef;f[0x4016]=0x80000b0e;f[0x4018]=2;current_time=2;
 assert(lpa_before(&activation,&cfg,&s,&host)&&activation.last_time==2);
 f[0x4002]|=1u<<28;assert(!lpa_before(&activation,&cfg,&s,&host)&&activation.failed);
 f[0x4002]&=~(1u<<28);assert(!lpa_before(&activation,&cfg,&s,&host));
 for(unsigned bad=0;bad<16;bad++){
 setup();activation=(struct lpa_state){0};assert(lpa_before(&activation,&cfg,&s,&host));
 if(bad==0)gia.domain++;
 if(bad==1)s.memory.owner.context=&activation;
 if(bad==2)s.hop.context=&activation;
 if(bad==3)cfg.clock_ctx=&activation;
 if(bad==4)current_time=0;
 if(bad==5)tables[3*4096]^=4;
 if(bad==6)s.platform.ctx=&activation;
 if(bad==7)s.memory.owner.map=map_alt;
 if(bad==8)s.platform.read_field=rd_alt;
 if(bad==9)s.hop.vmwrite=wr_alt;
 if(bad==10)cfg.now=now_alt;
 if(bad==11)s.memory.eptp^=0x1000;
 if(bad==12)s.host.caps.physical_bits++;
 if(bad==13)s.host.caps.timer_shift++;
 struct lvp_cpu_features changed=host;if(bad==14)changed.basic_edx^=1;
 struct guest_apic replacement=apic;if(bad==15)gia.apic=&replacement;
 assert(!lpa_before(&activation,&cfg,&s,&changed)&&activation.failed);
 setup();assert(!lpa_before(&activation,&cfg,&s,&host));
 }
 setup();activation=(struct lpa_state){0};assert(lpa_before(&activation,&cfg,&s,&host));
 struct lgs_session *other=malloc(sizeof *other);assert(other);*other=s;
 assert(!lpa_before(&activation,&cfg,other,&host)&&activation.failed);free(other);

 setup();activation=(struct lpa_state){0};readfail=true;assert(!lpa_before(&activation,&cfg,&s,&host)&&!activation.active&&activation.kind==LCP_BASELINE&&activation.failed);
 readfail=false;assert(!lpa_before(&activation,&cfg,&s,&host));
 free(ram);free(tables);puts("linux_profile_gate: actual controls/EPT/owned firmware/fresh-domain failure matrix PASS (host only, no activation)");return 0;
}
