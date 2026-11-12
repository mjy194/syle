#include "linux_vcpu_prepare.h"
#define B(n) (UINT32_C(1)<<(n))
static bool select_ctl(uint64_t caps,uint32_t want,uint32_t supported,uint32_t*out){
 uint32_t v=want|(uint32_t)caps;
 if((v&~(uint32_t)(caps>>32))||(v&~supported))return false;
 *out=v;return true;
}
static bool page(uint64_t p,unsigned bits){
 return p && !(p&4095) && p<=(UINT64_C(1)<<bits)-4096;
}
static bool fixed(uint64_t v,uint64_t must,uint64_t may){
 return (v&must)==must && !(v&~may);
}
static bool add(struct lvp_plan*p,uint32_t e,uint64_t v){
 if(p->count==LVP_MAX_FIELDS)return false;
 p->fields[p->count++]=(struct lvp_field){e,v};return true;
}
bool lvp_prepare(const struct linux_guest_memory*m,const struct lvp_caps*c,struct lvp_plan*out){
 if(!m||!c||!out||!m->prepared||m->plan.ram_size!=LGM_RAM_SIZE||
 !c->want_wbinvd||c->physical_bits<32||c->physical_bits>52||c->timer_shift>20)return false;
 const struct lb_entry_state*e=&m->entry;
 if(e->rip!=m->plan.entry_addr||e->rip!=m->plan.kernel_addr+0x200||
 m->plan.kernel_addr<0x1000000||m->plan.kernel_addr>=LGM_RAM_SIZE||
 m->plan.kernel_envelope<0x201||m->plan.kernel_envelope>LGM_RAM_SIZE-m->plan.kernel_addr||
 e->rsi!=0x90000||m->plan.params_addr!=e->rsi||e->rsp!=0x80000||
 e->cr0!=0x80000033||e->cr3!=0x1000||e->cr4!=0x620||e->efer!=0x500||
 e->gdt_base!=0x4000||e->gdt_limit!=47||e->cs!=0x10||e->ds!=0x18)return false;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(!page(m->pages[i],c->physical_bits))return false;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(!page(m->tables[i],c->physical_bits))return false;
 if(m->eptp!=(m->tables[0]|0x1e))return false;
 uint64_t cr4=e->cr4|B(13);
 if(!fixed(e->cr0,c->cr0_fixed0,c->cr0_fixed1)||!fixed(cr4,c->cr4_fixed0,c->cr4_fixed1))return false;
 const uint64_t ept_need=(1ull<<6)|(1ull<<14)|(1ull<<20)|(1ull<<25);
 if((c->ept&ept_need)!=ept_need)return false;
 uint32_t pin,pri,sec,en,ex;
 uint32_t pw=B(7)|B(9)|B(10)|B(11)|B(12)|B(15)|B(16)|B(19)|B(20)|B(23)|B(24)|B(29)|B(30)|B(31);
 uint32_t ew=B(2)|B(9)|B(14)|B(15),xw=B(2)|B(9)|(15u<<18);
 uint32_t sw=B(1)|B(6);
 /* Legacy mandatory-one reserved defaults are allowed, not requested. No
  * IO/MSR bitmaps, TPR shadow, VPID, unrestricted guest, virtual APIC etc. */
 if(!select_ctl(c->pin,0x49,0x5f,&pin)||
 !select_ctl(c->primary,pw,pw|0x0401e172u,&pri)||
 !select_ctl(c->secondary,sw,sw,&sec)||
 !select_ctl(c->entry,ew,ew|0x11ffu,&en)||
 !select_ctl(c->exit,xw,xw|0x36dffu,&ex))return false;
 struct lvp_plan p={0};
#define W(k,v) do{if(!add(&p,(k),(v)))return false;}while(0)
 W(0x4000,pin);W(0x4002,pri);W(0x401e,sec);W(0x4012,en);W(0x400c,ex);
 W(0x201a,m->eptp);W(0x4004,UINT32_MAX);W(0x4006,0);W(0x4008,0);
 W(0x400a,0);W(0x400e,0);W(0x4010,0);W(0x4014,0);W(0x4016,0);
 W(0x4018,0);W(0x401a,0);W(0x2010,0);
 W(0x6000,UINT64_MAX);W(0x6002,UINT64_MAX);W(0x6004,e->cr0);W(0x6006,e->cr4);
 W(0x2800,UINT64_MAX);W(0x2802,0);W(0x2804,UINT64_C(0x0007040600070406));W(0x2806,e->efer);
 for(unsigned i=0;i<8;i++){
  uint64_t sel=e->ds,ar=0xc093,limit=UINT32_MAX,base=0;
  if(i==1){sel=e->cs;ar=0xa09b;}
  if(i==6){sel=0;ar=0x10000;limit=0;}
  if(i==7){sel=0x20;ar=0x8b;limit=103;base=0x6000;}
  W(0x800+2*i,sel);W(0x4800+2*i,limit);W(0x4814+2*i,ar);W(0x6806+2*i,base);
 }
 W(0x4810,e->gdt_limit);W(0x4812,0);W(0x6816,e->gdt_base);W(0x6818,0);
 W(0x4824,0);W(0x4826,0);W(0x6822,0);
 W(0x6800,e->cr0);W(0x6802,e->cr3);W(0x6804,cr4);W(0x681a,0x400);
 W(0x681c,e->rsp);W(0x681e,e->rip);W(0x6820,2);
 W(0x482a,0);W(0x6824,0);W(0x6826,0);
 W(0x482e,UINT64_C(1)<<(28-c->timer_shift));
 p.gpr[4]=e->rsi;*out=p;return true;
#undef W
}
