#include "firmware_layout.h"
static bool sized(uint64_t n){return n && !(n&(FWL_PAGE-1));}
bool fwl_contains(const struct fwl_region*r,uint64_t a,uint64_t n){
 if(!r||!r->size||!n||r->base>UINT64_MAX-r->size||a<r->base)return false;
 uint64_t off=a-r->base;
 return off<r->size && n<=r->size-off;
}
bool fwl_plan_build(const struct fwl_request*r,struct fwl_plan*out){
 if(!r||!out||r->physical_bits<32||r->physical_bits>52||
 !sized(r->ram_size)||!sized(r->code_size)||!sized(r->vars_size)||
 r->ram_size>FWL_LOW_MMIO||r->code_size>FWL_TOP||
 r->vars_size>FWL_TOP-r->code_size)return false;
 uint64_t code=FWL_TOP-r->code_size,vars=code-r->vars_size;
 /* Leave the conventional 3-4GiB window available for future MMIO.
  * Flash may occupy its high end, never low RAM or the reset vector's hole. */
 if(vars<FWL_LOW_MMIO||vars<r->ram_size)return false;
 struct fwl_plan p={
  .ram={0,r->ram_size,FWL_RAM,true,true,true},
  .vars={vars,r->vars_size,FWL_FLASH_VARS,true,false,false},
  .code={code,r->code_size,FWL_ROM_CODE,true,false,true},
  .reset_linear=FWL_RESET,.reset_code_offset=FWL_RESET-code
 };
 if(!fwl_contains(&p.code,FWL_RESET,16))return false;
 *out=p;return true;
}
