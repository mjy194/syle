#include "firmware_pci_bus.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t ram[16384],disk[512];static bool fail;static unsigned commits;static uint64_t expected,reference;
static bool admit(void*x,uint64_t a,size_t n,bool w){(void)x;(void)w;return n&&a<sizeof ram&&n<=sizeof ram-a;}
static bool rd(void*x,uint64_t a,void*b,size_t n){if(!admit(x,a,n,false))return false;memcpy(b,ram+a,n);return true;}
static bool wt(void*x,uint64_t a,const void*b,size_t n){if(!admit(x,a,n,true))return false;memcpy(ram+a,b,n);return true;}
static void fence(void*x){(void)x;}
static bool commit(void*x,uint32_t f,uint64_t v){(void)x;assert(f==0x681e&&v==expected);commits++;return !fail;}
static void init(struct vfb*s){memset(s,0,sizeof*s);fail=false;reference=0;struct vro_io io={0,admit,rd,wt,fence};assert(vfb_init_projected(s,io,disk,sizeof disk,1,0,UINT64_MAX));}
static bool io(struct vfb*s,unsigned p,unsigned w,bool in,uint32_t*v){unsigned z=w==4?3:w==2?1:0,len=0;uint8_t b[2];if(w==2)b[len++]=0x66;b[len++]=(uint8_t)(0xec+(in?0:2)+(w==1?0:1));uint64_t g[15]={0};g[0]=*v;g[3]=p;expected=0x1000+len;bool ok=vfb_io(s,g,0x1000,0,((uint64_t)p<<16)|z|(in?8:0),len,1,reference,b,len,true,0,commit);if(ok)*v=(uint32_t)g[0];return ok;}
static void addr(struct vfb*s,unsigned b,unsigned o){uint32_t v=0x80000000|(b<<8)|(o&~3u);assert(io(s,0xcf8,4,false,&v));}
static uint32_t config(struct vfb*s,unsigned b,unsigned o,unsigned w){addr(s,b,o);uint32_t v=0;assert(io(s,0xcfc+(o&3),w,true,&v));return v;}
static bool writecfg(struct vfb*s,unsigned b,unsigned o,unsigned w,uint32_t v){addr(s,b,o);return io(s,0xcfc+(o&3),w,false,&v);}
int main(void){struct vfb s;init(&s);unsigned found[4],count=0;
 for(unsigned d=0;d<32;d++){
  unsigned b=d<<3;if(config(&s,b,0,2)==0xffff)continue;
  assert(count<4);found[count++]=b;unsigned max=(config(&s,b,14,1)&0x80)?8:1;
  for(unsigned f=1;f<max;f++)if(config(&s,b+f,0,2)!=0xffff){assert(count<4);found[count++]=b+f;}
 }
 assert(count==4&&found[0]==0&&found[1]==8&&found[2]==11&&found[3]==16);
 for(unsigned k=0;k<count;k++)for(unsigned w=1;w<=4;w*=2)for(unsigned o=0;o<256;o+=w){uint32_t v=config(&s,found[k],o,w),bytes=0;for(unsigned j=0;j<w;j++)bytes|=config(&s,found[k],o+j,1)<<(8*j);assert(v==bytes);}
 assert(config(&s,256,0,4)==UINT32_MAX);
 for(unsigned k=0;k<3;k++)for(unsigned o=16;o<40;o+=4){assert(writecfg(&s,found[k],o,4,UINT32_MAX));assert(config(&s,found[k],o,4)==0);}
 assert(writecfg(&s,11,0x41,1,0xb0)&&s.board.pm.pmba==0xb001);assert(writecfg(&s,11,0x80,4,1));uint32_t early=9;assert(s.projection.pm_command==0&&io(&s,0xb008,4,true,&early)&&early==0);assert(writecfg(&s,11,4,4,0xf900ffff));uint32_t v=0;assert(io(&s,0xb008,4,true,&v)&&v==0);
 assert(writecfg(&s,11,4,1,0));v=9;assert(io(&s,0xb008,4,true,&v)&&!s.failed&&v==0); /* legacy PM decode ignores PCI command */
 assert(writecfg(&s,11,0x80,1,0));v=9;assert(!io(&s,0xb008,4,true,&v)&&s.failed&&v==9);
 init(&s);addr(&s,11,0x40);v=0xb001;fail=true;assert(!io(&s,0xcfc,4,false,&v)&&s.failed&&s.board.pm.pmba==1);fail=false;unsigned before=commits;assert(!io(&s,0xcfc,4,false,&v)&&commits==before);
 init(&s);assert(!writecfg(&s,0,0x59,1,0x33)&&s.failed); /* unsupported PAM side effect */
 init(&s);v=0x1000;assert(io(&s,0xcf8,4,false,&v));assert(io(&s,0xcfc,4,true,&v)&&v==UINT32_MAX); /* enable clear */
 init(&s);assert(writecfg(&s,11,0x40,4,0xb001)&&writecfg(&s,11,4,1,1));v=9;assert(!io(&s,0xb008,4,true,&v)&&v==9&&s.failed); /* PMIOSE clear */
 init(&s);addr(&s,11,0x40);reference=10000000;v=0xb001;fail=true;assert(!io(&s,0xcfc,4,false,&v)&&s.failed&&s.board.pm.pmba==1);assert(s.board.pm.last==10000000&&s.board.pm.ticks==3579545); /* sample persists, config does not */
 puts("opt-in projected single CF8 enumeration + live PM decode: host PASS");
}
