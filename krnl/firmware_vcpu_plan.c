#include "firmware_vcpu_plan.h"
#define B(n) (UINT32_C(1)<<(n))
static bool ctl(uint64_t caps,uint32_t want,uint32_t supported,uint32_t*out){
 uint32_t v=want|(uint32_t)caps;
 if((v&~(uint32_t)(caps>>32))||(v&~supported))return false;
 *out=v;return true;
}
static bool add(struct lvp_plan*p,uint32_t e,uint64_t v){
 if(p->count==LVP_MAX_FIELDS)return false;
 p->fields[p->count++]=(struct lvp_field){e,v};return true;
}
bool fvp_reset_plan(const struct fwl_request*r,uint64_t eptp,const struct lvp_caps*c,struct lvp_plan*out){
 struct fwl_plan map;
 if(!r||!c||!out||!fwl_plan_build(r,&map)||r->physical_bits!=c->physical_bits||
 !c->want_wbinvd||c->timer_shift>20)return false;
 uint64_t root=eptp&~UINT64_C(4095);
 if((eptp&4095)!=0x1e||!root||root>((UINT64_C(1)<<c->physical_bits)-4096))return false;
 uint64_t cr0=UINT64_C(0x60000010)|(c->cr0_fixed0&~UINT64_C(0x80000001));
 uint64_t cr4=B(13);
 /* Only NE may differ from architectural reset under this probe profile. */
 if((cr0&~UINT64_C(0x60000030))||(cr0&~c->cr0_fixed1)||
 (cr4&c->cr4_fixed0)!=c->cr4_fixed0||(cr4&~c->cr4_fixed1))return false;
 const uint64_t need=(1ull<<6)|(1ull<<14)|(1ull<<20)|(1ull<<25);
 if((c->ept&need)!=need)return false;
 uint32_t pin,pri,sec,en,ex;
 uint32_t pw=B(7)|B(9)|B(10)|B(11)|B(12)|B(15)|B(16)|B(19)|B(20)|B(23)|B(24)|B(29)|B(30)|B(31);
 uint32_t ew=B(2)|B(14)|B(15),xw=B(2)|B(9)|(15u<<18),sw=B(1)|B(6)|B(7);
 if(!ctl(c->pin,0x49,0x5f,&pin)||!ctl(c->primary,pw,pw|0x0401e172u,&pri)||
 !ctl(c->secondary,sw,sw,&sec)||!ctl(c->entry,ew,ew|0x11ffu,&en)||
 !ctl(c->exit,xw,xw|0x36dffu,&ex))return false;
 struct lvp_plan p={0};
#define W(k,v) do{if(!add(&p,(k),(v)))return false;}while(0)
 W(0x4000,pin);W(0x4002,pri);W(0x401e,sec);W(0x4012,en);W(0x400c,ex);
 W(0x201a,eptp);W(0x4004,UINT32_MAX&~B(14));W(0x4006,0);W(0x4008,0);
 W(0x400a,0);W(0x400e,0);W(0x4010,0);W(0x4014,0);W(0x4016,0);
 W(0x4018,0);W(0x401a,0);W(0x2010,0);
 W(0x6000,UINT64_MAX);W(0x6002,UINT64_MAX);W(0x6004,0x60000010);W(0x6006,0);
 W(0x2800,UINT64_MAX);W(0x2802,0);W(0x2804,UINT64_C(0x0007040600070406));W(0x2806,0);
 for(unsigned i=0;i<8;i++){
  uint64_t sel=0,ar=0x93,limit=0xffff,base=0;
  if(i==1){sel=0xf000;ar=0x9b;base=0xffff0000;}
  if(i==6){ar=0x10000;limit=0;}
  if(i==7){ar=0x8b;}
  W(0x800+2*i,sel);W(0x4800+2*i,limit);W(0x4814+2*i,ar);W(0x6806+2*i,base);
 }
 W(0x4810,0xffff);W(0x4812,0xffff);W(0x6816,0);W(0x6818,0);
 W(0x4824,0);W(0x4826,0);W(0x6822,0);
 W(0x6800,cr0);W(0x6802,0);W(0x6804,cr4);W(0x681a,0x400);
 W(0x681c,0);W(0x681e,0xfff0);W(0x6820,2);
 W(0x482a,0);W(0x6824,0);W(0x6826,0);
 W(0x482e,UINT64_C(1)<<(28-c->timer_shift));
 p.gpr[3]=0x600; /* RDX reset processor signature, same family/model as profile. */
 *out=p;return true;
#undef W
}
