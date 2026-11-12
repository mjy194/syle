#include "guest_wait.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool gw_halt(void*c,gw_read r,gw_write w){
 if(!r||!w)return false;
 const uint32_t f[]={0x4402,0x440c,0x681e,0x0802,0x4816,0x4826,0x4824,0x4002,0x401e,0x6800,0x6804,0x2806,0x4012,0x6820,0x6822};
 uint64_t v[15];for(unsigned i=0;i<15;i++)if(!r(c,f[i],&v[i]))return false;
 if(v[0]!=12||v[1]!=1||!canonical(v[2])||v[2]==UINT64_MAX||!canonical(v[2]+1)||
 !v[3]||v[3]>65535||(v[3]&7)||(v[4]&~UINT64_C(1))!=0xa09a||v[5]||v[6]>2||
 v[7]>UINT32_MAX||(v[7]&0x80000080)!=0x80000080||v[8]!=0x42||
 (v[9]&0x80000031)!=0x80000031||(v[9]&~UINT64_C(0x8005003b))||
 (v[10]&0x2020)!=0x2020||(v[10]&~UINT64_C(0x26b0))||(v[11]&0x500)!=0x500||(v[11]&~UINT64_C(0xd01))||
 v[12]>UINT32_MAX||!(v[12]&0x200)||(v[13]&0x100)||v[14])return false;
 /* HLT completes the instruction following STI/MOV SS, clearing that block.
  * VM-entry hardware, not this emulator, wakes a halted guest on injection. */
 return w(c,0x4824,0)&&w(c,0x681e,v[2]+1)&&w(c,0x4826,1);
}
bool gw_window(void*c,gw_read r,gw_write w,uint32_t must_one,uint32_t allowed_one,bool enable){
 if(!r||!w||(must_one&4)||!(allowed_one&4))return false;
 uint64_t ctl;if(!r(c,0x4002,&ctl)||ctl>UINT32_MAX||!(ctl&0x80000000))return false;
 uint64_t next=enable?ctl|4:ctl&~UINT64_C(4);
 return next==ctl||w(c,0x4002,next);
}
bool gw_window_exit(void*c,gw_read r,gw_write w,uint32_t must_one,uint32_t allowed_one){
 if(!r||!w)return false;
 if((must_one&4)||!(allowed_one&4))return false;
 const uint32_t f[]={0x4402,0x4002,0x6820,0x4824,0x4826,0x4408};uint64_t v[6];
 for(unsigned i=0;i<6;i++)if(!r(c,f[i],&v[i]))return false;
 if(v[0]!=7||v[1]>UINT32_MAX||(v[1]&0x80000004)!=0x80000004||!(v[2]&512)||v[3]||v[4]||v[5])return false;
 return w(c,0x4002,v[1]&~UINT64_C(4));
}
