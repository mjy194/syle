#include "../firmware_cmos_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void reject(struct fwl_plan*p){struct fcmi_state s={0},old=s;assert(!fcmi_init(&s,p)&&!memcmp(&s,&old,sizeof s));}
int main(void){
 struct fwl_request r={0x8000000,3653632,540672,36};struct fwl_plan p;assert(fwl_plan_build(&r,&p));
 struct fcmi_state s={0};assert(fcmi_init(&s,&p));assert(s.above16_low==0&&s.above16_high==7);
 uint8_t lo=0xff,hi=0xff;assert(fcmi_read(&s,0x34,&lo)&&fcmi_read(&s,0x35,&hi));assert((((uint64_t)hi<<8|lo)<<16)+0x1000000==p.ram.size);
 for(unsigned i=0x5b;i<=0x5d;i++){uint8_t v=0xff;assert(fcmi_read(&s,i,&v)&&!v);}
 for(unsigned i=0;i<256;i++)if(i!=0x34&&i!=0x35&&(i<0x5b||i>0x5d)){uint8_t v=0xa5;assert(!fcmi_read(&s,i,&v)&&v==0xa5);}
 struct fcmi_state old=s;assert(!fcmi_init(&s,&p)&&!memcmp(&old,&s,sizeof s));
 uint8_t v=0xa5;struct fcmi_state zero={0};assert(!fcmi_read(&zero,0x34,&v)&&v==0xa5);assert(!fcmi_read(0,0x34,&v)&&!fcmi_read(&s,0x34,0));assert(!fcmi_init(0,&p)&&!fcmi_init(&zero,0));
 struct fwl_plan bad=p;uint64_t sizes[]={0,0x1000000,0x7ff0000,0x8000001,0x10000000,UINT64_MAX};
 for(unsigned i=0;i<sizeof sizes/sizeof*sizes;i++){bad=p;bad.ram.size=sizes[i];reject(&bad);}
 bad=p;bad.ram.base=4096;reject(&bad);bad=p;bad.ram.read=false;reject(&bad);bad=p;bad.ram.write=false;reject(&bad);bad=p;bad.ram.execute=false;reject(&bad);bad=p;bad.ram.kind=FWL_ROM_CODE;reject(&bad);
 /* A single canonical RAM region cannot encode hidden holes: truncation or
  * relocation rejects. Ownership holes in backing are caller's separate gate. */
 bad=p;bad.vars.base+=4096;reject(&bad);bad=p;bad.code.base-=4096;reject(&bad);
 bad=p;bad.vars.base=0x100000;reject(&bad);bad=p;bad.code.size=UINT64_MAX;reject(&bad);bad=p;bad.vars.size=UINT64_MAX;reject(&bad);
 bad=p;bad.code.write=true;reject(&bad);bad=p;bad.code.execute=false;reject(&bad);bad=p;bad.code.read=false;reject(&bad);bad=p;bad.code.kind=FWL_RAM;reject(&bad);
 bad=p;bad.vars.write=true;reject(&bad);bad=p;bad.vars.execute=true;reject(&bad);bad=p;bad.vars.read=false;reject(&bad);bad=p;bad.vars.kind=FWL_RAM;reject(&bad);
 bad=p;bad.reset_linear--;reject(&bad);bad=p;bad.reset_code_offset++;reject(&bad);
 /* No borrowed layout pointer: caller mutations do not rewrite size facts. */
 p.ram.size=0;assert(fcmi_read(&s,0x35,&v)&&v==7);struct fcmi_state clone=s;clone.above16_high=8;v=0xa5;assert(!fcmi_read(&clone,0x35,&v)&&v==0xa5);
 r.code_size=65536;r.vars_size=4096;assert(fwl_plan_build(&r,&p));memset(&zero,0,sizeof zero);assert(fcmi_init(&zero,&p));
 puts("firmware CMOS memory: actual-layout contract, 128MiB encoding, zero upper RAM, rejects PASS");return 0;
}
