#include "../firmware_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void reject(struct fwl_request r){
 struct fwl_plan p,b;memset(&p,0xa5,sizeof p);b=p;
 assert(!fwl_plan_build(&r,&p));assert(!memcmp(&p,&b,sizeof p));
}
int main(void){
 struct fwl_request r={128ull*1024*1024,3653632,540672,32};
 struct fwl_plan p;assert(fwl_plan_build(&r,&p));
 assert(p.code.base==0xffc84000&&p.vars.base==0xffc00000);
 assert(p.code.base+p.code.size==FWL_TOP);
 assert(p.vars.base+p.vars.size==p.code.base);
 assert(p.reset_code_offset==r.code_size-16);
 assert(p.ram.read&&p.ram.write&&p.ram.execute);
 assert(p.code.read&&!p.code.write&&p.code.execute);
 assert(p.vars.read&&!p.vars.write&&!p.vars.execute);
 assert(fwl_contains(&p.code,FWL_RESET,16));
 assert(!fwl_contains(&p.code,FWL_RESET,17));
 assert(!fwl_contains(&p.ram,r.ram_size,1));
 assert(!fwl_contains(&p.ram,0,0));
 assert(!fwl_contains(&p.ram,UINT64_MAX,2));
 assert(!fwl_contains(0,0,1));
 struct fwl_region bad={UINT64_MAX-1,4,FWL_RAM,true,true,true};
 assert(!fwl_contains(&bad,UINT64_MAX-1,1));
 struct fwl_request x=r;x.physical_bits=31;reject(x);x.physical_bits=53;reject(x);
 x=r;x.code_size=0;reject(x);x=r;x.vars_size=0;reject(x);x=r;x.ram_size=0;reject(x);
 x=r;x.ram_size++;reject(x);x=r;x.vars_size++;reject(x);x=r;x.code_size++;reject(x);
 x=r;x.code_size=UINT64_MAX;reject(x);x=r;x.code_size=FWL_TOP;reject(x);
 x=r;x.vars_size=FWL_TOP;reject(x);x=r;x.ram_size=FWL_LOW_MMIO+FWL_PAGE;reject(x);
 x=r;x.vars_size=UINT64_C(0x40000000);reject(x);
 x=r;x.physical_bits=52;assert(fwl_plan_build(&x,&p));
 x=r;x.ram_size=FWL_LOW_MMIO;x.code_size=FWL_PAGE;x.vars_size=FWL_PAGE;
 assert(fwl_plan_build(&x,&p));assert(p.code.base==FWL_TOP-FWL_PAGE);
 assert(p.vars.base==FWL_TOP-2*FWL_PAGE);
 x.vars_size=UINT64_C(0x40000000)-FWL_PAGE;
 assert(fwl_plan_build(&x,&p));assert(p.vars.base==FWL_LOW_MMIO);
 assert(p.ram.base+p.ram.size==p.vars.base);
 x.vars_size+=FWL_PAGE;reject(x);
 assert(!fwl_plan_build(0,&p));assert(!fwl_plan_build(&r,0));
 puts("firmware_layout: candidate map/bounds/permissions/reset/atomic rejection PASS (host only)");
}
