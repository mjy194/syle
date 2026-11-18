#include "firmware_boot_gp.h"
static bool canon(uint64_t v){return (v>>47)==0||(v>>47)==0x1ffff;}
static bool range(const struct fmm_memory*m,uint64_t v,size_t n,enum fmm_access a,uint8_t*out){
 if(!n||v>UINT64_MAX-(n-1)||!canon(v)||!canon(v+n-1))return false;
 for(size_t i=0;i<n;i++){uint64_t p;uint8_t b;
  if(!fmm_translate(m,v+i,a,&p)||!fwm_read_ram(m->memory,p,&b,1))return false;
  if(out)out[i]=b;
 }return true;
}
static uint64_t u64(const uint8_t*b){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)b[i]<<(i*8);return v;}
bool fbg_inject(struct fbg_state*s,const struct fmm_memory*m,const struct fbg_exit*e,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!s)return false;
 if(s->busy){s->failed=true;return false;}
 if(s->failed||s->pending||s->count>=8||!m||!e||!w||!(e->flags&2)||(e->flags&~UINT64_C(0x250ed7))||(e->flags&0x20100)||
 e->idt_limit<223||e->idt_limit>65535||e->gdt_limit>65535||!canon(e->idtr)||e->idtr>UINT64_MAX-e->idt_limit||!canon(e->idtr+e->idt_limit)||
 !canon(e->gdtr)||e->gdtr>UINT64_MAX-e->gdt_limit||!canon(e->gdtr+e->gdt_limit)||
 !m->cs_selector||(m->cs_selector&7)||m->cs_selector>e->gdt_limit||e->gdt_limit-m->cs_selector<7)return false;
 for(unsigned i=0;i<s->count;i++)if(s->rip[i]==e->rip&&s->cr3[i]==m->cr3)return false;
 uint8_t ins[2],g[16],d[8];
 if(!range(m,e->rip,2,FMM_EXEC,ins)||ins[0]!=15||ins[1]!=0x32||
 !range(m,e->idtr+13*16,16,FMM_READ,g)||!range(m,e->gdtr+m->cs_selector,8,FMM_READ,d))return false;
 uint64_t lo=u64(g),hi=u64(g+8),cs=u64(d);
 if(((lo>>16)&65535)!=m->cs_selector||((lo>>32)&65535)!=0x8e00||(hi>>32)||
 (cs!=UINT64_C(0x00af9b000000ffff)&&cs!=UINT64_C(0x00af9a000000ffff)))return false;
 uint64_t handler=(lo&65535)|((lo>>32)&UINT64_C(0xffff0000))|(hi<<32),top=e->rsp&~UINT64_C(15);
 if(top<48||!canon(e->rsp)||!range(m,handler,16,FMM_EXEC,0)||!range(m,top-48,48,FMM_WRITE,0))return false;
 s->busy=true;
 if(!w(c,0x6820,e->flags|0x10000)||s->failed||!w(c,0x4018,0)||s->failed||!w(c,0x401a,0)||s->failed||!w(c,0x4016,0x80000b0d)||s->failed){s->busy=false;s->failed=true;return false;}
 s->rip[s->count]=e->rip;s->cr3[s->count++]=m->cr3;s->pending=true;s->busy=false;return true;
}
bool fbg_after(struct fbg_state*s,uint32_t reason,uint32_t vector,uint32_t interruption,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!s)return false;
 if(s->failed||s->busy||!s->pending||!w||(vector&0x80000000u)||(interruption&0x80000000u)){s->failed=true;return false;}
 switch(reason){case 10:case 14:case 16:case 28:case 29:case 30:case 31:case 32:case 40:break;default:s->failed=true;return false;}
 s->busy=true;
 if(!w(c,0x4016,0)||s->failed||!w(c,0x4018,0)||s->failed||!w(c,0x401a,0)||s->failed){s->busy=false;s->failed=true;return false;}
 s->pending=false;s->busy=false;return true;
}
