#include "firmware_boot_cpu.h"
bool fboot_query(const struct fcp_host*h,bool source,uint32_t leaf,uint32_t sub,struct fcp_result*out){
 if(!out||!h||(h->basic_edx&0x2008)!=0x2008||!(h->extended_edx&0x800))return false;
 if(!source||!fcats_supported(h))return false;
 struct fcp_result r={0};
 if(leaf==0x15||(leaf>0x15&&leaf<0x80000000u)||leaf>0x80000008u){
  *out=(struct fcp_result){1,1,10000000,0};return true;
 }
 if(leaf>1&&leaf<0x80000000u){*out=r;return true;}
 if(!fcats_query(h,source,leaf,sub,&r))return false;
 if(leaf==0)r.a=0x15;
 if(leaf==1||(leaf>1&&leaf<0x80000000u)||leaf>0x80000008u)r.d|=0x2008;
 if(leaf==0x80000001)r.d|=0x800;
 *out=r;return true;
}
bool fboot_efer_allowed(uint64_t value){return !(value&~UINT64_C(0x501));}
bool fboot_cr_encoding(uint64_t q,unsigned len,const uint8_t*b,size_t n){
 if(!b||n<len||(q&~UINT64_C(0xf3f)))return false;
 unsigned at=0,rex=0;if(len==4){rex=b[at++];if((rex&0xf0)!=0x40||(rex&2))return false;}else if(len!=3)return false;
 unsigned access=(q>>4)&3;if(access>1||b[at++]!=0x0f||b[at++]!=(access?0x20:0x22))return false;
 unsigned mod=b[at];return (mod&0xc0)==0xc0&&(((mod>>3)&7)+((rex&4)?8:0))==(q&15)&&((mod&7)+((rex&1)?8:0))==((q>>8)&15);
}

bool fboot_preemption_resume(uint64_t reason,uint64_t cs,uint64_t flags,uint64_t blocking,uint64_t pending,uint64_t dr7,uint64_t event,uint64_t vector){
 return (flags&2)&&!(flags&~UINT64_C(0x3d7fd7))&&reason==52&&cs&&cs<=UINT16_MAX&&
 ((cs&7)==0||(cs&7)==3)&&!(flags&0x100)&&!blocking&&!pending&&dr7==0x400&&!event&&!vector;
}
