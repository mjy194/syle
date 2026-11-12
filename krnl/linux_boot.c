#include "linux_boot.h"
static uint16_t u16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t*p){return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
static uint64_t u64(const uint8_t*p){return (uint64_t)u32(p)|((uint64_t)u32(p+4)<<32);}
static void put(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static bool writer_ok(const struct lb_writer*w){return w&&w->write&&w->zero;}
bool lb_inspect(const void*image,size_t size,size_t rd,const char*cmd,struct lb_plan*out){
 if(!image||!out||!cmd||size<0x268||!rd||rd>=LB_RAM_SIZE)return false;
 const uint8_t*b=image;
 size_t hend=0x202+(size_t)b[0x201];
 if(b[0x200]!=0xeb||hend<0x268||hend>0x290||hend>size)return false;
 if(u16(b+0x1fe)!=0xaa55||u32(b+0x202)!=0x53726448||u16(b+0x206)<0x20c||!(b[0x211]&1)||!b[0x234]||!(u16(b+0x236)&1))return false;
 uint64_t off=((uint64_t)(b[0x1f1]?b[0x1f1]:4)+1)*512;
 uint64_t payload=(uint64_t)u32(b+0x1f4)*16,env=u32(b+0x260),align=u32(b+0x230);
 if(off>size||payload<0x201||payload>size-off||env<payload||!align||(align&(align-1))||align>LB_RAM_SIZE)return false;
 uint64_t base=16ull*1024*1024,pref=u64(b+0x258);
 if(pref>=base&&pref<LB_RAM_SIZE&&!(pref&(align-1)))base=pref;
 else base=(base+align-1)&~(align-1);
 if(base>=LB_RAM_SIZE||env>LB_RAM_SIZE-base)return false;
 uint64_t ceiling=(uint64_t)u32(b+0x22c)+1;
 if(ceiling>LB_RAM_SIZE)ceiling=LB_RAM_SIZE;
 if(rd>ceiling)return false;
 uint64_t rda=(ceiling-rd)&~4095ull;
 if(rda<base+env)return false;
 size_t n=0;uint32_t cap=u32(b+0x238);
 if(cap>4095)cap=4095;
 while(n<cap&&cmd[n])n++;
 if(!cap||n==cap)return false;
 *out=(struct lb_plan){LB_RAM_SIZE,base,payload,env,rda,rd,0x90000,0x91000,base+0x200,off,n+1};return true;
}
bool lb_load(const void*image,size_t size,const void*initrd,size_t rd,const char*cmd,const struct lb_writer*w,struct lb_plan*out){
 struct lb_plan p;
 if(!initrd||!out||!writer_ok(w)||!lb_inspect(image,size,rd,cmd,&p))return false;
 const uint8_t*b=image;uint8_t bp[4096]={0};
 /* Header end is setup jump target, clipped to fields we implement. */
 size_t end=0x202+(size_t)b[0x201];
 if(b[0x200]!=0xeb||end<0x268||end>0x290)return false;
 for(size_t i=0x1f1;i<end;i++)bp[i]=b[i];
 bp[0x210]=0xff;bp[0x211]|=0x80;
 put(bp+0x214,p.kernel_addr,4);put(bp+0x218,p.initrd_addr,4);put(bp+0x21c,rd,4);
 put(bp+0x228,p.cmdline_addr,4);put(bp+0x250,0,8);
 /* Preserve low RAM while protecting live bootstrap objects until Linux
  * replaces its initial page tables, stack and boot parameter storage. */
 const uint64_t bounds[]={0,0x1000,0x5000,0x6000,0x7000,0x7f000,0x80000,0x90000,0x92000,0x9f000,0x100000,LB_RAM_SIZE};
 bp[0x1e8]=11;
 for(unsigned i=0;i<11;i++){
  uint8_t*r=bp+0x2d0+i*20;
  put(r,bounds[i],8);put(r+8,bounds[i+1]-bounds[i],8);
  put(r+16,(i&1)?2:1,4);
 }
 if(!w->zero(w->context,p.kernel_addr,p.kernel_envelope)||!w->write(w->context,p.kernel_addr,b+p.payload_offset,p.kernel_size)||!w->write(w->context,p.initrd_addr,initrd,rd)||!w->write(w->context,p.params_addr,bp,sizeof bp)||!w->write(w->context,p.cmdline_addr,cmd,p.cmdline_size))return false;
 *out=p;return true;
}
bool lb_entry_prepare(const struct lb_plan*p,const struct lb_writer*w,struct lb_entry_state*out){
 if(!p||!out||!writer_ok(w)||p->ram_size!=LB_RAM_SIZE||p->params_addr!=0x90000||p->kernel_addr<0x1000000||p->kernel_addr>=LB_RAM_SIZE||p->kernel_size<=0x200||p->kernel_envelope<p->kernel_size||p->kernel_envelope>LB_RAM_SIZE-p->kernel_addr||p->entry_addr!=p->kernel_addr+0x200)return false;
 if(!w->zero(w->context,0x1000,0x4000))return false;
 uint8_t q[8];put(q,0x2003,8);if(!w->write(w->context,0x1000,q,8))return false;
 put(q,0x3003,8);if(!w->write(w->context,0x2000,q,8))return false;
 for(unsigned i=0;i<64;i++){put(q,((uint64_t)i<<21)|0x83,8);if(!w->write(w->context,0x3000+i*8,q,8))return false;}
 put(q,0x00af9a000000ffffull,8);if(!w->write(w->context,0x4010,q,8))return false;
 put(q,0x00cf92000000ffffull,8);if(!w->write(w->context,0x4018,q,8))return false;
 /* Dedicated guest TSS: busy long-mode descriptor, no IST or I/O bitmap. */
 if(!w->zero(w->context,0x6000,4096))return false;
 put(q,UINT64_C(0x00008b0060000067),8);if(!w->write(w->context,0x4020,q,8))return false;
 put(q,0x80000,8);if(!w->write(w->context,0x6004,q,8))return false;
 put(q,104,2);if(!w->write(w->context,0x6066,q,2))return false;
 *out=(struct lb_entry_state){p->entry_addr,p->params_addr,0x80000,0x80000033,0x1000,0x620,0x500,0x4000,47,0x10,0x18};return true;
}
