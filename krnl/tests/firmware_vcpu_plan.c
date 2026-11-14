#include "../firmware_vcpu_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct lvp_plan p;
static uint64_t val(unsigned e){for(unsigned i=0;i<p.count;i++)if(p.fields[i].encoding==e)return p.fields[i].value;assert(0);return 0;}
static struct fwl_request r={128ull*1024*1024,3653632,540672,36};
static uint64_t eptp=0x1000001e;
static void reject(struct lvp_caps c){struct lvp_plan old;memset(&p,0xa5,sizeof p);old=p;assert(!fvp_reset_plan(&r,eptp,&c,&p));assert(!memcmp(&p,&old,sizeof p));}
int main(void){
 struct lvp_caps c={.pin=0xffffffff00000000ull,.primary=0xffffffff00000000ull,
 .secondary=0xffffffff00000000ull,.entry=0xffffffff00000000ull,.exit=0xffffffff00000000ull,
 .ept=(1ull<<6)|(1ull<<14)|(1ull<<20)|(1ull<<25),.cr0_fixed0=0x80000021,
 .cr0_fixed1=0xffffffff,.cr4_fixed0=0x2000,.cr4_fixed1=0xffffffff,
 .physical_bits=36,.timer_shift=5,.want_wbinvd=true};
 assert(fvp_reset_plan(&r,eptp,&c,&p));assert(p.count<=LVP_MAX_FIELDS);
 for(unsigned i=0;i<p.count;i++){assert((p.fields[i].encoding&0xc00)!=0xc00);for(unsigned j=0;j<i;j++)assert(p.fields[i].encoding!=p.fields[j].encoding);}
 assert(val(0x401e)==0xc2);assert(!(val(0x4012)&(1<<9)));
 assert(val(0x6800)==0x60000030&&val(0x6004)==0x60000010);
 assert(val(0x6804)==0x2000&&val(0x6006)==0);
 assert(val(0x802)==0xf000&&val(0x6808)==0xffff0000&&val(0x681e)==0xfff0);
 assert(val(0x6808)+val(0x681e)==FWL_RESET);assert(val(0x4816)==0x9b);
 assert(val(0x2806)==0&&val(0x6802)==0&&val(0x6820)==2);
 assert(!(val(0x4004)&(1<<14))&&val(0x4006)==0&&val(0x4008)==0);
 assert(p.gpr[3]==0x600);for(unsigned i=0;i<15;i++)if(i!=3)assert(!p.gpr[i]);
 struct lvp_caps x=c;x.secondary&=~(1ull<<39);reject(x);
 x=c;x.secondary&=~(1ull<<33);reject(x);x=c;x.secondary&=~(1ull<<38);reject(x);
 x=c;x.entry|=1<<9;reject(x);x=c;x.secondary|=1<<5;reject(x);
 x=c;x.cr0_fixed0|=1<<16;reject(x);x=c;x.cr4_fixed0|=1<<5;reject(x);
 x=c;x.cr4_fixed1&=~0x2000;reject(x);x=c;x.physical_bits=35;reject(x);
 x=c;x.timer_shift=21;reject(x);x=c;x.want_wbinvd=false;reject(x);
 x=c;x.ept&=~(1ull<<25);reject(x);
 eptp|=0x40;reject(c);eptp=0x1e;reject(c);eptp=(1ull<<36)|0x1e;reject(c);
 eptp=0x1000001e;r.code_size=0;reject(c);r.code_size=3653632;
 assert(!fvp_reset_plan(0,eptp,&c,&p)&&!fvp_reset_plan(&r,eptp,0,&p)&&!fvp_reset_plan(&r,eptp,&c,0));
 puts("firmware_vcpu_plan: reset/UG/EPT/control/state/rejection PASS (host only, not VM entry)");
}
