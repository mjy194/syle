#include "linux_pt_permissions.h"
bool lpp_range(uint64_t cr3,uint64_t va,uint64_t size,unsigned access,uint64_t cr0,uint64_t cr4,uint64_t efer,uint64_t ram,void*ctx,bool(*read)(void*,uint64_t,uint64_t*)){
 if(!size||size>65536||va>UINT64_MAX-(size-1)||access>2||!(cr0&(1ULL<<31))||!(cr4&32)||(cr4&~0x26b0ULL)||(efer&0x500)!=0x500||(efer&~0xd01ULL))return false;
 uint64_t end=va+size-1;
 for(uint64_t p=va;;){
  struct lpt_trace t;lpt_walk(cr3,p,ram,ctx,read,&t);if(t.status!=1)return false;
  bool rw=true,nx=false;
  for(unsigned i=0;i<t.count;i++){rw=rw&&!!(t.entry[i]&2);nx=nx||!!(t.entry[i]>>63);}
  if((nx&&!(efer&0x800))||(access==2&&nx)||(access==1&&!rw&&(cr0&(1u<<16))))return false;
  uint64_t remaining=4096-(p&4095);if(end-p<remaining)return true;
  p+=remaining;
 }
}
