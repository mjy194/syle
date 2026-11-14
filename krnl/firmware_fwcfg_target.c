#include "firmware_fwcfg_target.h"
/* Caller already proved this walk via fmm_translate. Repeat only to collect
 * table ownership, under the same frozen snapshot, never to relax admission. */
static bool disjoint_tables(const struct fmm_memory*m,uint64_t va,uint64_t target,uint64_t target_token){
 uint64_t table=m->cr3&UINT64_C(0xfffff000);
 for(unsigned level=0;level<4;level++){
  if(table>FWM_RAM_SIZE-4096||table==(target&~UINT64_C(4095)))return false;
  uint64_t token=m->memory->ram[table/4096];
  if(!fwm_owned_page(m->memory,token)||token==target_token)return false;
  uint8_t bytes[8];uint64_t entry=0;
  if(!fwm_read_ram(m->memory,table+((va>>(39-9*level))&511)*8,bytes,8))return false;
  for(unsigned i=0;i<8;i++)entry|=(uint64_t)bytes[i]<<(i*8);
  if(level==3||((entry&128)&&level==2))return true;
  table=entry&UINT64_C(0xfffff000);
 }
 return false;
}
bool fft_prepare(const struct fmm_memory*m,uint64_t va,unsigned count,uint64_t rip,struct fft_plan*out){
 if(!out||!count||count>64||(va&4095)+count>4096||va>UINT64_MAX-(count-1))return false;
 uint64_t base;
 if(!fmm_translate(m,va,FMM_WRITE,&base)||base>=FWM_RAM_SIZE||count>FWM_RAM_SIZE-base||
    (base&4095)+count>4096)return false;
 for(unsigned i=1;i<count;i++){
  uint64_t p;if(!fmm_translate(m,va+i,FMM_WRITE,&p)||p!=base+i)return false;
 }
 uint64_t token=m->memory->ram[base/4096];
 if(!fwm_owned_page(m->memory,token)||!disjoint_tables(m,va,base,token))return false;
 /* REP INSB admission supplies the two-byte instruction start. Safely
  * fetch both bytes before checking their backing pages and walk paths. */
 uint8_t instruction[15];
 if(rip==UINT64_MAX||fmm_fetch(m,rip,instruction)<2)return false;
 for(unsigned i=0;i<2;i++){
  uint64_t source;if(!fmm_translate(m,rip+i,FMM_EXEC,&source)||
    (source&~UINT64_C(4095))==(base&~UINT64_C(4095))||
    !disjoint_tables(m,rip+i,base,token))return false;
  uint64_t source_token;
  if(source<FWM_RAM_SIZE)source_token=m->memory->ram[source/4096];
  else {
   uint64_t index=(source-m->memory->layout.vars.base)/4096;
   if(index>=m->memory->flash_pages||index>=FWM_FLASH_PAGES)return false;
   source_token=m->memory->flash[index];
  }
  if(!fwm_owned_page(m->memory,source_token)||source_token==token)return false;
 }
 uint8_t*bytes=m->memory->owner.map(m->memory->owner.context,token);
 if(!bytes||((uintptr_t)bytes&4095))return false;
 *out=(struct fft_plan){bytes+(base&4095),base,count};return true;
}
