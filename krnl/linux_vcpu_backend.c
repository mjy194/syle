#include "linux_vcpu_backend.h"
#include "linux_vcpu_native.h"
#include "paging.h"
static uint64_t get64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(i*8);return v;}
bool lvb_descriptor(const uint8_t*g,size_t bytes,uint16_t limit,uint16_t sel,
 uint32_t*ar,uint32_t*lim,uint64_t*base){
 if(!g||!ar||!lim||!base||!sel||(sel&7)||sel+7u>limit||bytes<(size_t)sel+8)return false;
 uint64_t d=get64(g+sel);uint32_t a=((d>>40)&255)|((d>>40)&0xf000);
 uint32_t l=(d&0xffff)|((d>>32)&0xf0000);if(a&0x8000)l=(l<<12)|4095;
 uint64_t b=((d>>16)&0xffffff)|((d>>32)&0xff000000);
 if(!(a&16)){if(sel+15u>limit||bytes<(size_t)sel+16)return false;
  uint64_t hi=get64(g+sel+8);if(hi>>32)return false;b|=hi<<32;}
 *ar=a;*lim=l;*base=b;return true;
}
#ifndef LVB_HOST_TEST
static bool canon(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
struct table {uint16_t limit;uint64_t base;}__attribute__((packed));
static bool execution(void){uint16_t cs;uint64_t f;
 __asm__ volatile("mov %%cs,%0":"=r"(cs));if(cs&3)return false;
 __asm__ volatile("pushfq;pop %0":"=r"(f));
 return !(f&((1u<<9)|(1u<<10)))&&!paging_ap_start_attempted();
}
static bool usable(struct lvb_context*c){return c&&c->bound&&execution();}
static bool physical(struct lvb_context*c,uint64_t p){return p&&!(p&4095)&&!(p>>c->physical_bits);}
static bool vmxe(void){uint64_t x;__asm__ volatile("mov %%cr4,%0":"=r"(x));return (x&(1u<<13))!=0;}
static void cpuid(uint32_t n,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d){
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(n),"c"(0));
}
static bool read_msr(void*v,uint32_t n,uint64_t*out){return usable(v)&&lvn_read_msr(NULL,n,out);}
static bool capture(void*v,struct lvh_host*out){
 struct lvb_context*c=v;if(!out||!usable(c)||c->active||!vmxe())return false;
 struct lvh_host h={0};struct table g,i;uint16_t ldtr;
 __asm__ volatile("mov %%cr0,%0":"=r"(h.cr0));__asm__ volatile("mov %%cr3,%0":"=r"(h.cr3));
 __asm__ volatile("mov %%cr4,%0":"=r"(h.cr4));__asm__ volatile("pushfq;pop %0":"=r"(h.flags));
 __asm__ volatile("mov %%dr7,%0":"=r"(h.dr7));
 if(h.dr7!=0x400||(h.cr0&12)||!(h.cr0&1)||!(h.cr0&(1ull<<31))||
    (h.cr4&((1u<<12)|(1u<<17)|(1u<<18)|(1u<<22)|(1u<<23)))||
    !(h.cr4&(1u<<5))||!(h.cr4&(1u<<9))||(h.cr3&0xfe7)||(h.cr3>>c->physical_bits))return false;
 __asm__ volatile("sgdt %0":"=m"(g));__asm__ volatile("sidt %0":"=m"(i));
 __asm__ volatile("sldt %0":"=r"(ldtr));if(ldtr)return false;
 if(!g.base||!i.base||!canon(g.base)||!canon(i.base)||g.base>UINT64_MAX-g.limit||
    i.base>UINT64_MAX-i.limit||!canon(g.base+g.limit)||!canon(i.base+i.limit))return false;
 h.gdt_base=g.base;h.gdt_limit=g.limit;h.idt_base=i.base;h.idt_limit=i.limit;
 __asm__ volatile("mov %%es,%0":"=r"(h.es));__asm__ volatile("mov %%cs,%0":"=r"(h.cs));
 __asm__ volatile("mov %%ss,%0":"=r"(h.ss));__asm__ volatile("mov %%ds,%0":"=r"(h.ds));
 __asm__ volatile("mov %%fs,%0":"=r"(h.fs));__asm__ volatile("mov %%gs,%0":"=r"(h.gs));
 __asm__ volatile("str %0":"=r"(h.tr));
 const uint8_t*bytes=(const uint8_t*)(uintptr_t)g.base;uint32_t ar,limit;uint64_t base;
 if(!lvb_descriptor(bytes,(size_t)g.limit+1,g.limit,h.cs,&ar,&limit,&base)||ar!=0xa09b||base||limit!=UINT32_MAX)return false;
 if(!lvb_descriptor(bytes,(size_t)g.limit+1,g.limit,h.ss,&ar,&limit,&base)||ar!=0xc093||base||limit!=UINT32_MAX)return false;
 uint16_t data[]={h.es,h.ds,h.fs,h.gs};
 for(unsigned n=0;n<4;n++)if(data[n]&&(!lvb_descriptor(bytes,(size_t)g.limit+1,g.limit,data[n],&ar,&limit,&base)||ar!=0xc093||base||limit!=UINT32_MAX))return false;
 if(!lvb_descriptor(bytes,(size_t)g.limit+1,g.limit,h.tr,&h.tr_access,&h.tr_limit,&h.tr_base)||
    h.tr_access!=0x8b||h.tr_limit<103||!h.tr_base||!canon(h.tr_base)||
    h.tr_base>UINT64_MAX-h.tr_limit||!canon(h.tr_base+h.tr_limit))return false;
 *out=h;return true;
}
static bool on(void*v,uint64_t p){
 struct lvb_context*c=v;if(!usable(c)||c->active||c->root||c->current||!physical(c,p)||!vmxe())return false;
 unsigned char bad;__asm__ volatile("vmxon %1;setna %0":"=qm"(bad):"m"(p):"cc","memory");
 if(bad)return false;
 c->root=p;c->active=true;return true;
}
static bool clear(void*v,uint64_t p){
 struct lvb_context*c=v;if(!usable(c)||!c->active||!physical(c,p)||p==c->root||!vmxe())return false;
 unsigned char bad;__asm__ volatile("vmclear %1;setna %0":"=qm"(bad):"m"(p):"cc","memory");
 if(bad)return false;
 if(c->current==p)c->current=0;
 return true;
}
static bool load(void*v,uint64_t p){
 struct lvb_context*c=v;if(!usable(c)||!c->active||!physical(c,p)||p==c->root||
 (c->current&&c->current!=p)||!vmxe())return false;
 unsigned char bad;__asm__ volatile("vmptrld %1;setna %0":"=qm"(bad):"m"(p):"cc","memory");
 if(bad)return false;
 c->current=p;return true;
}
static bool write(void*v,uint32_t f,uint64_t value){
 struct lvb_context*c=v;if(!usable(c)||!c->active||!c->current||!vmxe())return false;
 unsigned char bad;__asm__ volatile("vmwrite %2,%1;setna %0":"=qm"(bad):"r"((uint64_t)f),"r"(value):"cc","memory");return !bad;
}
static bool invalidate(void*v,uint64_t eptp){
 struct lvb_context*c=v;if(!usable(c)||!c->active||!vmxe()||
 (eptp&4095)!=0x1e||!physical(c,eptp&~UINT64_C(4095)))return false;
 struct{uint64_t eptp,reserved;}__attribute__((aligned(16)))d={eptp,0};unsigned char bad;
 __asm__ volatile("invept %1,%2;setna %0":"=qm"(bad):"m"(d),"r"(UINT64_C(1)):"cc","memory");return !bad;
}
static bool off(void*v){
 struct lvb_context*c=v;if(!usable(c)||!c->active||c->current||!vmxe())return false;
 unsigned char bad;__asm__ volatile("vmxoff;setna %0":"=qm"(bad)::"cc","memory");
 if(bad)return false;
 c->active=false;c->root=0;return true;
}
bool lvb_bind(struct lvb_context*c,const struct sg_memory_ops*m,struct lvh_ops*out){
 if(!c||!m||!out||c->bound||c->active||c->root||c->current||!execution()||
 !m->allocate||!m->release||!m->map||!m->writeback)return false;
 uint32_t a,b,x,d;cpuid(0,&a,&b,&x,&d);
 if(a<1||b!=0x756e6547||d!=0x49656e69||x!=0x6c65746e)return false;
 cpuid(1,&a,&b,&x,&d);if(!(x&(1u<<5))||!(d&(1u<<5)))return false;
 cpuid(0x80000000,&a,&b,&x,&d);if(a<0x80000008)return false;
 cpuid(0x80000008,&a,&b,&x,&d);unsigned bits=a&255;if(bits<32||bits>52)return false;
 c->physical_bits=bits;c->bound=true;
 *out=(struct lvh_ops){.context=c,.memory=*m,.physical_bits=bits,.intel_vmx=true,
 .capture=capture,.read_msr=read_msr,.vmxon=on,.vmclear=clear,.vmptrld=load,
 .vmwrite=write,.invept=invalidate,.vmxoff=off};return true;
}
#endif
