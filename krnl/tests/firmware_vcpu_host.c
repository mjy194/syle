#include "../firmware_vcpu_host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct firmware_memory ram;
static struct lvh_context c;
struct mock {unsigned calls,fail,alloc,live,writes;bool used[2],active;unsigned block;uint32_t enc[200];uint64_t val[200];_Alignas(4096) unsigned char page[2][4096];};
static struct mock m;
static bool tick(void){return ++m.calls!=m.fail;}
static bool capture(void*x,struct lvh_host*h){(void)x;if(!tick())return false;*h=(struct lvh_host){.cr0=0x80000033,.cr3=0x2000,.cr4=0x2620,.flags=2,.tr_base=0x8000,.gdt_base=0x9000,.idt_base=0xa000,.dr7=0x400,.tr_access=0x8b,.tr_limit=103,.cs=8,.ss=16,.ds=16,.es=16,.tr=24,.gdt_limit=39,.idt_limit=4095};return true;}
static bool rd(void*x,uint32_t n,uint64_t*v){(void)x;if(!tick())return false;switch(n){case 0x3a:*v=5;break;case 0x480:*v=1ull|(4096ull<<32)|(6ull<<50)|(1ull<<55);break;case 0x48d:case 0x48e:case 0x48f:case 0x490:case 0x48b:*v=0xffffffff00000000ull;break;case 0x48c:*v=(1ull<<6)|(1ull<<14)|(1ull<<20)|(1ull<<25);break;case 0x485:*v=5;break;case 0x486:*v=0x21;break;case 0x487:case 0x489:*v=0xffffffff;break;case 0x488:*v=0x2000;break;case 0x277:*v=0x0007040600070406;break;case 0xc0000080:*v=0x500;break;case 0xc0000100:case 0xc0000101:case 0x174:case 0x175:case 0x176:case 0x1d9:*v=0;break;default:assert(0);}return true;}
static unsigned idx(uint64_t p){assert(p==0x100000||p==0x101000);return (p-0x100000)/4096;}
static uint64_t alloc(void*x){(void)x;if(!tick())return 0;assert(m.alloc<2);unsigned i=m.alloc++;m.used[i]=true;m.live++;return 0x100000+i*4096;}
static bool release(void*x,uint64_t p){(void)x;unsigned i=idx(p);assert(!m.active&&m.used[i]);if(m.block==4)return false;m.used[i]=false;m.live--;return true;}
static void*map(void*x,uint64_t p){(void)x;if(!tick())return 0;return m.page[idx(p)];}
static bool wb(void*x,uint64_t p){(void)x;assert(m.used[idx(p)]);return tick();}
static bool on(void*x,uint64_t p){(void)x;assert(m.used[idx(p)]&&!m.active);if(!tick())return false;m.active=true;return true;}
static bool clear(void*x,uint64_t p){(void)x;assert(m.active&&m.used[idx(p)]);return tick()&&m.block!=1;}
static bool load(void*x,uint64_t p){(void)x;assert(m.active&&m.used[idx(p)]);return tick();}
static bool write(void*x,uint32_t e,uint64_t v){(void)x;assert(m.active);if(!tick())return false;assert(m.writes<200);m.enc[m.writes]=e;m.val[m.writes++]=v;return true;}
static bool inv(void*x,uint64_t e){(void)x;assert(m.active&&e==ram.eptp);return tick()&&m.block!=2;}
static bool off(void*x){(void)x;assert(m.active);if(!tick()||m.block==3)return false;m.active=false;return true;}
static struct lvh_ops ops={.memory={0,alloc,release,map,wb},.physical_bits=36,.intel_vmx=true,.capture=capture,.read_msr=rd,.vmxon=on,.vmclear=clear,.vmptrld=load,.vmwrite=write,.invept=inv,.vmxoff=off};
static uint64_t value(unsigned e){for(unsigned i=0;i<m.writes;i++)if(m.enc[i]==e)return m.val[i];assert(0);return 0;}
static void reset(void){assert(!m.active&&!m.live);memset(&m,0,sizeof m);memset(&c,0,sizeof c);}
int main(void){
 ram.prepared=true;struct fwl_request r={128ull*1024*1024,65536,4096,36};
 assert(fwl_plan_build(&r,&ram.layout));ram.eptp=0x3000001e;
 assert(fwh_open(&c,&ops,&ram,0x80000,0x400000));unsigned steps=m.calls;
 assert(c.ready&&c.active&&m.live==2);assert(value(0x6c02)==0x2000&&value(0xc0c)==24&&value(0x6c16)==0x400000&&value(0x2c02)==0x500);
 assert(m.page[0][0]==1&&m.page[1][0]==1);assert(lvh_close(&c)&&!m.active&&!m.live);
 for(unsigned f=1;f<=steps;f++){reset();m.fail=f;assert(!fwh_open(&c,&ops,&ram,0x80000,0x400000));assert(!m.active&&!m.live&&!c.ready);}
 for(unsigned b=1;b<=4;b++){reset();assert(fwh_open(&c,&ops,&ram,0x80000,0x400000));m.block=b;assert(!lvh_close(&c));assert(m.live==2);assert(c.active==(b!=4));assert(!c.ready);m.block=0;assert(lvh_close(&c));assert(!m.live&&!m.active);}
 reset();ops.physical_bits=53;assert(!fwh_open(&c,&ops,&ram,0x80000,0x400000));assert(!m.calls);ops.physical_bits=36;assert(!fwh_open(&c,&ops,&ram,0x80001,0x400000));assert(!m.calls);
 printf("PASS firmware reset host VMCS model: %u open failure positions, clear/invept/off/free retention, host fields; no hardware\n",steps);return 0;
}
