#include "firmware_observer_stack.h"
static bool canonical(uint64_t a){return a<=UINT64_C(0x7fffffffffff)||a>=UINT64_C(0xffff800000000000);}
static bool ram_token(const struct firmware_memory*m,unsigned page){
 if(page>=FWM_RAM_PAGES||m->flash_pages>FWM_FLASH_PAGES)return false;
 uint64_t token=m->ram[page];if(!fwm_owned_page(m,token))return false;
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(i!=page&&m->ram[i]==token)return false;
 for(unsigned i=0;i<m->flash_pages;i++)if(m->flash[i]==token)return false;
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(m->tables[i]==token)return false;
 return true;
}
bool fos_read_word(const struct fmm_memory*m,uint64_t va,uint64_t*out){
 if(!m||!m->memory||!out||!canonical(va)||va>UINT64_MAX-7||!canonical(va+7))return false;
 uint64_t first=0;
 for(unsigned i=0;i<8;i++){uint64_t gpa;
  if(!fmm_translate(m,va+i,FMM_READ,&gpa)||gpa>=FWM_RAM_SIZE||
     (i&&gpa!=first+i))return false;
  if(!i)first=gpa;
 }
 if(!ram_token(m->memory,(unsigned)(first/4096))||
    !ram_token(m->memory,(unsigned)((first+7)/4096)))return false;
 uint8_t bytes[8];if(!fwm_read_ram(m->memory,first,bytes,8))return false;
 uint64_t value=0;for(unsigned i=0;i<8;i++)value|=(uint64_t)bytes[i]<<(8*i);
 *out=value;return true;
}
static bool executable(const struct fmm_memory*m,uint64_t rip){uint8_t bytes[15];return fmm_fetch(m,rip,bytes)>0;}
bool fos_observe(const struct fmm_memory*m,uint64_t rbp,uint64_t rsp,bool top,struct fos_result*out){
 if(!m||!out)return false;
 struct fos_result r={.stop=FOS_END,.top_requested=top};
 if(top){r.top_readable=fos_read_word(m,rsp,&r.top_return);if(r.top_readable)r.top_executable=executable(m,r.top_return);}
 while(rbp&&r.count<FOS_MAX_FRAMES){
  if((rbp&7)||!canonical(rbp)||rbp>UINT64_MAX-15||!canonical(rbp+15)){r.stop=FOS_BAD_ADDRESS;break;}
  struct fos_frame f={.frame=rbp};
  if(!fos_read_word(m,rbp,&f.previous)||!fos_read_word(m,rbp+8,&f.return_rip)){r.stop=FOS_UNREADABLE;break;}
  f.return_executable=executable(m,f.return_rip);r.frames[r.count++]=f;
  if(!f.previous){r.stop=FOS_END;break;}
  if((f.previous&7)||f.previous<=rbp||!canonical(f.previous)){r.stop=FOS_CHAIN;break;}
  rbp=f.previous;if(r.count==FOS_MAX_FRAMES)r.stop=FOS_LIMIT;
 }
 *out=r;return true;
}
