#include "linux_mmio_memory.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool mode(const struct lmm_memory*m){return m&&m->read&&m->ram_size>=4096&&m->ram_size<=UINT64_C(0x100000000)&&!(m->ram_size&4095)&&
 (m->cr0&0x80000031)==0x80000031&&!(m->cr0&~UINT64_C(0x8005003b))&&
 (m->cr4&32)&&!(m->cr4&~UINT64_C(0x26b0))&&
 (m->efer&0x500)==0x500&&!(m->efer&~UINT64_C(0xd01))&&
 !(m->cr3&~UINT64_C(0xfffff018))&&!m->cs_base&&m->cs_selector&&!(m->cs_selector&7)&&
 (m->cs_ar&~1u)==0xa09a;}
static bool read64(const struct lmm_memory*m,uint64_t p,uint64_t*out){
 if(p>m->ram_size-8)return false;
 uint8_t b[8];if(!m->read(m->ctx,p,b,8))return false;
 uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)b[i]<<(i*8);*out=v;return true;
}
bool lmm_translate(const struct lmm_memory*m,uint64_t va,enum lmm_access access,uint64_t*out){
 if(!out||!mode(m)||!canonical(va)||access>LMM_EXEC||(int)access<0)return false;
 uint64_t table=m->cr3&UINT64_C(0xfffff000);bool rw=true,nx=false;
 for(unsigned level=0;level<4;level++){
  if(table>m->ram_size-4096)return false;
  unsigned shift=39-9*level;uint64_t e;
  if(!read64(m,table+((va>>shift)&511)*8,&e)||!(e&1)||!(e&32)||
     (e&UINT64_C(0x7fffffff00000000)))return false;
  rw=rw&&!!(e&2);nx=nx||!!(e>>63);
  if((nx&&!(m->efer&0x800))||(access==LMM_EXEC&&nx)||
     (access==LMM_WRITE&&!rw&&(m->cr0&(1u<<16))))return false;
  bool huge=(e&128)&&level==2;
  if((e&128)&&level<2)return false;
  if(level==3||huge){
   if(access==LMM_WRITE&&!(e&64))return false;
   uint64_t mask=huge?UINT64_C(0x1fffff):4095;
   if(huge&&(e&UINT64_C(0x1fe000)))return false;
   *out=(e&UINT64_C(0xfffff000)&~mask)|(va&mask);return true;
  }
  table=e&UINT64_C(0xfffff000);
 }
 return false;
}
size_t lmm_fetch(const struct lmm_memory*m,uint64_t rip,uint8_t b[15]){
 if(!b)return 0;
 size_t n=0;for(;n<15;n++){
  if(rip>UINT64_MAX-n)break;
  uint64_t p;if(!lmm_translate(m,rip+n,LMM_EXEC,&p)||p>=m->ram_size||!m->read(m->ctx,p,&b[n],1))break;
 }
 return n;
}
bool lmm_validate(const struct lmm_memory*m,uint64_t va,unsigned width,bool write,
 uint32_t reason,uint64_t q,uint64_t gla,uint64_t gpa,uint32_t vector,uint64_t base,uint64_t size){
 if(!mode(m)||base<m->ram_size||base>=UINT64_C(0x100000000)||size>UINT64_C(0x100000000)-base||
    reason!=48||vector||q!=(write?UINT64_C(0x182):UINT64_C(0x181))||va!=gla||
    (width!=1&&width!=2&&width!=4&&width!=8)||!size||base>UINT64_MAX-(size-1)||
    gpa<base||gpa-base>=size||size-(gpa-base)<width||va>UINT64_MAX-(width-1)||
    (va&4095)+width>4096||gpa>UINT64_MAX-(width-1))return false;
 for(unsigned i=0;i<width;i++){
  uint64_t p;if(!lmm_translate(m,va+i,write?LMM_WRITE:LMM_READ,&p)||p!=gpa+i)return false;
 }
 return true;
}
