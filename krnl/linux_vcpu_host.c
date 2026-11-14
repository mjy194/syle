#include "linux_vcpu_host.h"
static bool canon(uint64_t v){uint64_t x=v>>47;return x==0||x==0x1ffff;}
static bool fixed(uint64_t v,uint64_t lo,uint64_t hi){return (v&lo)==lo&&!(v&~hi);}
static bool rd(struct lvh_context*c,uint32_t n,uint64_t*v){return c->owner.read_msr(c->owner.context,n,v);}
static bool ops_ok(const struct lvh_ops*o,sg_allocate_role_fn allocate_role){return o&&o->intel_vmx&&o->physical_bits>=32&&o->physical_bits<=52&&o->capture&&o->read_msr&&o->vmxon&&o->vmclear&&o->vmptrld&&o->vmwrite&&o->invept&&o->vmxoff&&(allocate_role||o->memory.allocate)&&o->memory.release&&o->memory.map&&o->memory.writeback;}
static bool caps(struct lvh_context*c){
 uint64_t feature,misc;struct lvp_caps*p=&c->caps;
 if(!rd(c,0x3a,&feature)||(feature&5)!=5||!rd(c,0x480,&c->basic))return false;
 uint64_t b=c->basic;unsigned size=(b>>32)&0x1fff;
 if((b&0x80000000u)||!size||size>4096||((b>>50)&15)!=6)return false;
 bool t=!!(b&(1ull<<55));
 if(!rd(c,t?0x48d:0x481,&p->pin)||!rd(c,t?0x48e:0x482,&p->primary)||!rd(c,0x48b,&p->secondary)||!rd(c,t?0x490:0x484,&p->entry)||!rd(c,t?0x48f:0x483,&p->exit)||!rd(c,0x48c,&p->ept)||!rd(c,0x485,&misc)||!rd(c,0x486,&p->cr0_fixed0)||!rd(c,0x487,&p->cr0_fixed1)||!rd(c,0x488,&p->cr4_fixed0)||!rd(c,0x489,&p->cr4_fixed1))return false;
 p->physical_bits=c->owner.physical_bits;p->timer_shift=misc&31;p->want_wbinvd=true;return true;
}
static bool snapshot(struct lvh_context*c){
 struct lvh_host*h=&c->host;
 if(!c->owner.capture(c->owner.context,h))return false;
 uint64_t sc;
 if(!rd(c,0x277,&h->pat)||!rd(c,0xc0000080,&h->efer)||!rd(c,0xc0000100,&h->fs_base)||!rd(c,0xc0000101,&h->gs_base)||!rd(c,0x174,&sc)||sc>UINT32_MAX||!rd(c,0x175,&h->sysenter_esp)||!rd(c,0x176,&h->sysenter_eip)||!rd(c,0x1d9,&h->debugctl))return false;
 h->sysenter_cs=(uint32_t)sc;
 uint16_t segs[]={h->es,h->cs,h->ss,h->ds,h->fs,h->gs,h->tr};
 for(unsigned i=0;i<7;i++)if(segs[i]&7)return false;
 if(!h->cs||!h->ss||!h->tr||h->tr_access!=0x8b||h->tr_limit<103||h->tr+15u>h->gdt_limit)return false;
 if(h->flags&((1u<<9)|(1u<<10))||h->dr7!=0x400||h->debugctl||!(h->cr4&(1u<<13))||(h->cr4&((1u<<12)|(1u<<17)|(1u<<18)|(1u<<22)|(1u<<23))))return false;
 if(!(h->cr0&1)||!(h->cr0&(1ull<<31))||(h->cr0&12)||!(h->cr4&(1u<<9))||(h->efer&0x500)!=0x500)return false;
 if(!fixed(h->cr0,c->caps.cr0_fixed0,c->caps.cr0_fixed1)||!fixed(h->cr4,c->caps.cr4_fixed0,c->caps.cr4_fixed1))return false;
 if(!canon(h->fs_base)||!canon(h->gs_base)||!canon(h->tr_base)||!canon(h->gdt_base)||!canon(h->idt_base)||!canon(h->sysenter_esp)||!canon(h->sysenter_eip))return false;
 if((h->cr3&0xfe7u)||(h->cr3>>c->caps.physical_bits))return false;
 for(unsigned i=0;i<8;i++){unsigned type=(h->pat>>(8*i))&255;if(type!=0&&type!=1&&type!=4&&type!=5&&type!=6&&type!=7)return false;}
 return true;
}
static bool region(struct lvh_context*c,const struct linux_guest_memory*m,uint64_t*p,enum sg_memory_role role,sg_allocate_role_fn allocate_role){
 struct sg_memory_ops*o=&c->owner.memory;*p=allocate_role?allocate_role(o->context,role):o->allocate(o->context);
 /* Untrusted cross-owner overlap must be detected BEFORE WB/map/zero.
  * A broken allocator cannot authorize release of someone else's page. */
 uint64_t base=*p&~UINT64_C(4095);
 if(*p){
  bool alias=(p==&c->vmcs_page&&base==(c->vmxon_page&~UINT64_C(4095)));
  for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(base==m->pages[i])alias=true;
  for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(base==m->tables[i])alias=true;
  if(alias){c->quarantined=true;return false;}
 }
 unsigned bits=(c->basic&(1ull<<48))?32:c->caps.physical_bits;
 if(!*p||(*p&4095)||*p>((1ull<<bits)-4096)||!o->writeback(o->context,*p))return false;
 unsigned char*v=o->map(o->context,*p);if(!v||((uintptr_t)v&7))return false;
 for(unsigned i=0;i<4096;i++)v[i]=0;
 uint32_t rev=c->basic&0x7fffffff;for(unsigned i=0;i<4;i++)v[i]=(unsigned char)(rev>>(8*i));return true;
}
static bool close_impl(struct lvh_context*c,bool (*barrier)(void *),void *ctx){
 if(!c)return false;
 c->ready=false;
 if(c->quarantined)return false;
 if(c->active){
  if(!c->owner.vmclear(c->owner.context,c->vmcs_page))return false;
  if(!c->owner.invept(c->owner.context,c->eptp))return false;
  if(!c->owner.vmxoff(c->owner.context))return false;
  c->active=false;
 }
 /* VMXOFF has completed (or VMX was never entered). The trusted caller
  * additionally owns exclusion of entry/async producers. A failed barrier
  * retains pages, but must not cause VMX instructions to be replayed. */
 if(barrier&&!barrier(ctx))return false;
 bool ok=true;struct sg_memory_ops*o=&c->owner.memory;
 if(c->vmcs_page){if(o->release&&o->release(o->context,c->vmcs_page))c->vmcs_page=0;else ok=false;}
 if(c->vmxon_page){if(o->release&&o->release(o->context,c->vmxon_page))c->vmxon_page=0;else ok=false;}
 return ok;
}
bool lvh_close(struct lvh_context *c){return close_impl(c,0,0);}
bool lvh_close_with_barrier(struct lvh_context *c,bool (*barrier)(void *),void *ctx){
 return barrier&&close_impl(c,barrier,ctx);
}
static bool open_context(struct lvh_context*c,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip,sg_allocate_role_fn allocate_role){
 if(c)c->open_stage=1;
 if(!c||!ops_ok(o,allocate_role)||!m||c->quarantined||c->active||c->ready||c->vmxon_page||c->vmcs_page||!rsp||!rip||(rsp&15)||!canon(rsp)||!canon(rip))return false;
 c->owner=*o;c->eptp=m->eptp;
 c->open_stage=2;if(!caps(c))return false;
 c->open_stage=3;if(!snapshot(c))return false;
 c->open_stage=4;if(!lvp_prepare(m,&c->caps,&c->plan))return false;
 c->open_stage=5;
 if(!region(c,m,&c->vmxon_page,SG_MEMORY_VMXON,allocate_role)||!region(c,m,&c->vmcs_page,SG_MEMORY_VMCS,allocate_role))goto fail;
 c->open_stage=6;
 if(!o->vmxon(o->context,c->vmxon_page))goto fail;
 c->active=true;
 c->open_stage=7;
 if(!o->vmclear(o->context,c->vmcs_page)||!o->vmptrld(o->context,c->vmcs_page))goto fail;
 c->open_stage=8;
 for(unsigned i=0;i<c->plan.count;i++)if(!o->vmwrite(o->context,c->plan.fields[i].encoding,c->plan.fields[i].value))goto fail;
 c->open_stage=9;
 struct lvh_host*h=&c->host;
#define W(k,v) do{if(!o->vmwrite(o->context,(k),(v)))goto fail;}while(0)
 W(0xc00,h->es);W(0xc02,h->cs);W(0xc04,h->ss);W(0xc06,h->ds);W(0xc08,h->fs);W(0xc0a,h->gs);W(0xc0c,h->tr);
 W(0x4c00,h->sysenter_cs);W(0x2c00,h->pat);W(0x2c02,h->efer);
 W(0x6c00,h->cr0);W(0x6c02,h->cr3);W(0x6c04,h->cr4);W(0x6c06,h->fs_base);W(0x6c08,h->gs_base);W(0x6c0a,h->tr_base);W(0x6c0c,h->gdt_base);W(0x6c0e,h->idt_base);W(0x6c10,h->sysenter_esp);W(0x6c12,h->sysenter_eip);W(0x6c14,rsp);W(0x6c16,rip);
#undef W
 c->ready=true;c->open_stage=10;return true;
fail:
 (void)lvh_close(c);return false;
}
bool lvh_open(struct lvh_context*c,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip){
 return open_context(c,o,m,rsp,rip,0);
}
bool lvh_open_roles(struct lvh_context*c,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip,sg_allocate_role_fn allocate_role){
 return allocate_role&&open_context(c,o,m,rsp,rip,allocate_role);
}
