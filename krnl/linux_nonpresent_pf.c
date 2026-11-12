#include "linux_nonpresent_pf.h"
#include "linux_guest_session.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool phys(const struct lmm_memory*m,uint64_t a,uint64_t*v){
 uint8_t b[8];if(a>m->ram_size-8||!m->read(m->ctx,a,b,8))return false;
 *v=0;for(unsigned i=0;i<8;i++)*v|=(uint64_t)b[i]<<(8*i);return true;
}
static bool range(const struct lmm_memory*m,uint64_t va,size_t n,enum lmm_access access,uint8_t*out){
 if(!n||va>UINT64_MAX-(n-1)||!canonical(va)||!canonical(va+n-1))return false;
 for(size_t i=0;i<n;i++){uint64_t pa;
 if(!lmm_translate(m,va+i,access,&pa)||pa>=m->ram_size)return false;
 if(out&&!m->read(m->ctx,pa,out+i,1))return false;
 }return true;
}
static uint64_t u64(const uint8_t*b){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)b[i]<<(8*i);return v;}
static bool absent(const struct lmm_memory*m,uint64_t va,bool write){
 uint64_t table=m->cr3&UINT64_C(0xfffff000);bool rw=true;
 for(unsigned level=0;level<4;level++){
 if(table>m->ram_size-4096)return false;
 uint64_t e;if(!phys(m,table+((va>>(39-9*level))&511)*8,&e))return false;
 if(!(e&1))return true; /* Other bits in nonpresent entries are software-owned. */
 if(!(e&32)||(e&UINT64_C(0x7fffffff00000000))||((e>>63)&&!(m->efer&0x800)))return false;
 rw=rw&&!!(e&2);if(write&&!rw&&(m->cr0&(1u<<16)))return false;
 if(level==3||(e&128))return false; /* Present leaf/huge page cannot prove absence. */
 table=e&UINT64_C(0xfffff000);
 }return false;
}
bool lnp_validate(const struct lnp_state*s,const struct lmm_memory*m,const struct lnp_exit*e){
 if(!s||!m||!e||s->failed||s->pending||s->count>=LNP_MAX_FAULTS||e->reason||e->info!=0x80000b0e||
 (e->error!=0&&e->error!=2)||!canonical(e->address)||e->event||e->entry_error||e->entry_length||
 e->vectoring||e->activity||e->blocking||e->idt_limit<239||e->idt_limit>65535||e->gdt_limit>65535||
 !canonical(e->idtr)||e->idtr>UINT64_MAX-e->idt_limit||!canonical(e->idtr+e->idt_limit)||
 !canonical(e->gdtr)||e->gdtr>UINT64_MAX-e->gdt_limit||!canonical(e->gdtr+e->gdt_limit)||
 m->cs_selector>e->gdt_limit||e->gdt_limit-m->cs_selector<7)return false;
 for(unsigned i=0;i<s->count;i++){const struct lnp_tuple*t=&s->history[i];
 if(t->rip==e->rip&&t->address==e->address&&t->cr3==m->cr3&&t->error==e->error)return false;}
 /* The instruction mapping also validates the strict CPU paging/mode profile. */
 if(!range(m,e->rip,1,LMM_EXEC,0)||!absent(m,e->address,e->error==2))return false;
 uint8_t gate[16],desc[8];
 if(!range(m,e->idtr+14*16,16,LMM_READ,gate)||!range(m,e->gdtr+m->cs_selector,8,LMM_READ,desc))return false;
 uint64_t lo=u64(gate),hi=u64(gate+8),cs=u64(desc);
 if(((lo>>16)&65535)!=m->cs_selector||((lo>>32)&65535)!=0x8e00||(hi>>32)||
 (cs!=UINT64_C(0x00af9b000000ffff)&&cs!=UINT64_C(0x00af9a000000ffff)))return false;
 uint64_t handler=(lo&65535)|((lo>>32)&UINT64_C(0xffff0000))|(hi<<32);
 uint64_t top=e->rsp&~UINT64_C(15);if(top<48||!canonical(e->rsp))return false;
 return range(m,handler,16,LMM_EXEC,0)&&range(m,top-48,48,LMM_WRITE,0);
}
bool lnp_observe(const struct lnp_state*s,uint64_t reason,uint64_t vector,uint64_t event,struct lnp_state*out){
 if(!s||!out||s->failed||s->count>LNP_MAX_FAULTS||(s->pending&&!s->count)||(reason&(1ull<<31))||vector||
 (s->pending?(event!=0&&event!=0xb0e&&event!=0x80000b0e):event!=0))return false;
 if(s->pending&&reason!=10&&reason!=28&&reason!=30&&reason!=31&&reason!=32&&reason!=48)return false;
 *out=*s;out->pending=false;return true;
}
static bool owned(void*p,uint64_t g,void*out,size_t n){
 struct lgs_session*s=p;if(!n||g>=LGM_RAM_SIZE||n>LGM_RAM_SIZE-g)return false;uint8_t*b=out;
 for(size_t i=0;i<n;i++){uint64_t pa=s->memory.pages[(g+i)/4096];if(!pa||(pa&4095)||(pa>>52))return false;
 const uint8_t*q=s->memory.owner.map(s->memory.owner.context,pa);if(!q)return false;b[i]=q[(g+i)&4095];}return true;
}
static bool field(struct lgs_session*s,uint32_t f,uint64_t*out){return s->platform.read_field(s->platform.ctx,f,out);}
bool lnp_inject(struct lnp_state*state,struct lgs_session*s){
 if(!state||state->failed)return false;
 if(!s||!s->prepared||s->failed||!s->memory.prepared||!s->memory.owner.map||!s->platform.read_field||!s->hop.vmwrite||
 !s->state.ready||!s->state.guest_valid||s->state.host_dirty)goto fail;
 struct lnp_exit e;struct lmm_memory m={.ctx=s,.read=owned,.ram_size=LGM_RAM_SIZE};
 uint64_t cs,ar;
 if(!field(s,0x6800,&m.cr0)||!field(s,0x6802,&m.cr3)||!field(s,0x6804,&m.cr4)||!field(s,0x2806,&m.efer)||
 !field(s,0x6808,&m.cs_base)||!field(s,0x0802,&cs)||cs>65535||!field(s,0x4816,&ar)||ar>UINT32_MAX)goto fail;
 m.cs_selector=cs;m.cs_ar=ar;
 uint64_t*dst[]={&e.reason,&e.info,&e.error,&e.address,&e.rip,&e.rsp,&e.idtr,&e.idt_limit,&e.gdtr,&e.gdt_limit,&e.event,&e.entry_error,&e.entry_length,&e.vectoring,&e.activity,&e.blocking};
 const uint32_t fields[]={0x4402,0x4404,0x4406,0x6400,0x681e,0x681c,0x6818,0x4812,0x6816,0x4810,0x4016,0x4018,0x401a,0x4408,0x4826,0x4824};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)if(!field(s,fields[i],dst[i]))goto fail;
 if(!lnp_validate(state,&m,&e))goto fail;
 if(!s->hop.vmwrite(s->hop.context,0x4018,e.error)||!s->hop.vmwrite(s->hop.context,0x401a,0)||
 !s->hop.vmwrite(s->hop.context,0x4016,0x80000b0e))goto fail;
 state->history[state->count++]=(struct lnp_tuple){e.rip,e.address,m.cr3,e.error};state->pending=true;s->native.cr2=e.address;return true;
fail:state->failed=true;return false;
}
