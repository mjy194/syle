#include "firmware_piix_pm.h"
#include "firmware_cpuhp_absent.h"
#include "firmware_port92.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool fpp_init(struct fpp_state*s,uint64_t e,uint64_t r,uint64_t gap){
 if(!s||s->failed||s->pm.ready)return false;
 s->pci.address=0;return fpm_init(&s->pm,e,r,gap);
}
static bool access(struct fpp_state*s,uint64_t q,uint64_t*rax,uint64_t epoch,uint64_t ref){
 unsigned size=q&7,w=size==0?1:size==1?2:size==3?4:0,port=(q>>16)&0xffff;
 bool input=!!(q&8);
 if(!w)return false;
 if(port==0x92)return fp92_io((uint16_t)port,w,input,rax);
 if(port==0xaf00||port==0xaf05)return fcha_io((uint16_t)port,w,input,rax);
#ifdef SYLE_FPM_SCI_CONTROL
 if(port==0xb004){
  uint32_t value=(uint16_t)*rax;
  if(!fpm_io(&s->pm,epoch,ref,port,w,input,&value))return false;
  if(input)*rax=(*rax&~UINT64_C(0xffff))|value;
  return true;
 }
#endif
 if(port==0xb008){uint32_t value=(uint32_t)*rax;if(!fpm_io(&s->pm,epoch,ref,port,w,input,&value))return false;*rax=value;return true;}
 uint32_t address=s->pci.address;
 if(port>=0xcfc&&port<=0xcff&&(address&0x80000000)&&(address&0x00ffff00)==0x00000b00){
  if((port&(w-1))||w>0xd00-port)return false;
  unsigned off=(address&0xfc)+(port-0xcfc);uint32_t value=(uint32_t)*rax;
  if(w<4)value&=(1u<<(w*8))-1;
  if(!fpm_config(&s->pm,off,w,!input,&value))return false;
  if(input){uint64_t mask=w==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(w*8))-1;*rax=w==4?value:(*rax&~mask)|value;}
  return true;
 }
 return fpi_exit(&s->pci,q,rax);
}
bool fpp_encoding(uint64_t q,unsigned len,const uint8_t*b,unsigned n){
 if(!b||!len||len>3||n<len||(q&~UINT64_C(0xffff004b)))return false;
 unsigned size=q&7;if(size!=0&&size!=1&&size!=3)return false;
 bool word=size==1,input=!!(q&8),imm=!!(q&64);
 unsigned at=0;if(word){if(b[at++]!=0x66)return false;}
 unsigned opcode=(imm?0xe4:0xec)+(input?0:2)+(size==0?0:1);
 if(at>=len||b[at++]!=opcode)return false;
 if(imm){if(((q>>16)&0xffff)>255||at>=len||b[at++]!=(uint8_t)(q>>16))return false;}
 return at==len;
}
bool fpp_io(struct fpp_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t q,unsigned len,uint64_t epoch,uint64_t ref,const uint8_t*instruction,unsigned available,bool exclusive,void*ctx,bool(*wr)(void*,uint32_t,uint64_t)){
 if(!s||s->failed||!exclusive)return false;
 if(!fpm_sample(&s->pm,epoch,ref)){s->failed=true;return false;}
 if(!fpp_encoding(q,len,instruction,available)||!g||!wr||cpl||(q&~UINT64_C(0xffff004b))||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len)||
 ((q&64)?(len!=((q&7)==1?3u:2u)||((q>>16)&0xffff)>255):(len!=((q&7)==1?2u:1u)||(uint16_t)g[3]!=(uint16_t)(q>>16)))){s->failed=true;return false;}
 struct fpp_state next=*s;uint64_t value=g[0];
 if(!access(&next,q,&value,epoch,ref)||!wr(ctx,0x681e,rip+len)){s->failed=true;return false;}
 *s=next;g[0]=value;return true;
}
