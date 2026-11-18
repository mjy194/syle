#include "firmware_console.h"
#include "firmware_piix_pm.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool fcon_config(struct fcon_state*s,unsigned off,unsigned width,bool input,uint64_t*v){
 if(!s||s->failed||!v||(width!=1&&width!=2&&width!=4)||(off&(width-1))||off>=256||width>256-off)return false;
 struct fcon_state n=*s;uint32_t value=0;
 /* Synthetic Syle serial function: class07/subclass00/progIF02. */
 const uint8_t hdr[16]={0x36,0x1b,0x02,0x00,n.command,0,0,0,0,2,0,7,0,0,0,0};
 if(input){
  for(unsigned i=0;i<width;i++){
   unsigned p=off+i;uint8_t b=0;
   if(p<16)b=hdr[p];
   else if(p>=16&&p<20)b=(uint8_t)((n.probe?0xfffffff9u:(n.bar|1u))>>((p-16)*8));
   value|=(uint32_t)b<<(i*8);
  }
 }else if(off==16&&width==4){
  n.probe=(uint32_t)*v==UINT32_MAX;
  if(!n.probe)n.bar=(uint32_t)*v&0xfff8u;
 }else if(off>=4&&off+width<=6){
  /* Implement IO decode only; MEM/BME remain hardwired off. */
  if(off==4)n.command=(uint8_t)*v&1;
 } /* All other config bytes are RO / unimplemented, zero masks. */
 if(input){uint64_t mask=width==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(width*8))-1;*v=width==4?value:(*v&~mask)|value;}
 *s=n;return true;
}
bool fcon_port(const struct fcon_state*s,uint16_t p){return s&&!s->failed&&(s->command&1)&&!s->probe&&s->bar&&p>=s->bar&&(uint32_t)p-s->bar<8;}
bool fcon_io(struct fcon_state*s,uint64_t g[15],uint64_t rip,uint64_t q,unsigned len,const uint8_t*b,size_t available,void*x,bool(*write)(void*,uint32_t,uint64_t),bool*emit,uint8_t*byte){
 if(!s||s->failed)return false;
 uint16_t port=(uint16_t)(q>>16);bool input=(q&8)!=0;
 if(!g||!write||!emit||!byte||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len)||
    (q&~UINT64_C(0xffff0008))||len!=1||!b||available<1||b[0]!=(input?0xec:0xee)||
    (uint16_t)g[3]!=port||!fcon_port(s,port))return false;
 struct fcon_state n=*s;uint8_t value=(uint8_t)g[0],tx=0;unsigned reg=port-n.bar;
 if(!input&&reg==1&&!(n.uart.lcr&128)&&value)return false;
 if(!linux_uart_access(&n.uart,reg,!input,&value))return false;
 bool emitted=linux_uart_drain(&n.uart,&tx);
 if(!write(x,0x681e,rip+len)){s->failed=true;return false;}
 if(input){g[0]=(g[0]&~UINT64_C(255))|value;n.reads++;}else n.writes++;
 *s=n;*emit=emitted;*byte=tx;return true;
}
bool fcon_receive(struct fcon_state*s,uint8_t b){
 if(!s||s->failed||s->uart.rc>=((s->uart.fcr&1)?16u:1u)||(s->uart.mcr&16))return false;
 bool accepted=false;
 if(!linux_uart_receive(&s->uart,b,&accepted)||!accepted)return false;
 s->received++;return true;
}
