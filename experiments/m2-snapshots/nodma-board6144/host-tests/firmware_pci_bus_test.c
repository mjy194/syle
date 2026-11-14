#include "firmware_pci_bus.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t ram[32768],disk[1024];static unsigned pubs,commits;static bool commit_fail;static uint64_t expected;
static bool admit(void*x,uint64_t a,size_t n,bool w){(void)x;(void)w;return n&&a<sizeof ram&&n<=sizeof ram-a;}
static bool rd(void*x,uint64_t a,void*b,size_t n){if(!admit(x,a,n,false))return false;memcpy(b,ram+a,n);return true;}
static bool wt(void*x,uint64_t a,const void*b,size_t n){if(!admit(x,a,n,true))return false;memcpy(ram+a,b,n);return true;}
static void fence(void*x){(void)x;pubs++;}
static bool commit(void*x,uint32_t f,uint64_t v){(void)x;assert(f==0x681e&&v==expected);commits++;return !commit_fail;}
static void init(struct vfb*s){memset(s,0,sizeof*s);memset(ram,0,sizeof ram);pubs=commits=0;commit_fail=false;struct vro_io io={0,admit,rd,wt,fence};assert(vfb_init(s,io,disk,sizeof disk,1,0,UINT64_MAX));}
static bool io(struct vfb*s,uint64_t*g,unsigned port,unsigned w,bool input){unsigned z=w==4?3:w==2?1:0;uint8_t b[2];unsigned len=0;if(w==2)b[len++]=0x66;b[len++]=(uint8_t)(0xec+(!input?2:0)+(w==1?0:1));g[3]=port;expected=0x1000+len;return vfb_io(s,g,0x1000,0,((uint64_t)port<<16)|z|(input?8:0),len,1,0,b,len,true,0,commit);}
static void address(struct vfb*s,uint64_t*g,uint32_t a){g[0]=a;assert(io(s,g,0xcf8,4,false));assert(s->board.pci.address==(a&0x80fffffc));}
static void cfgout(struct vfb*s,uint64_t*g,unsigned off,unsigned w,uint32_t v){address(s,g,0x80001000|(off&~3u));g[0]=v;assert(io(s,g,0xcfc+(off&3),w,false));}
static void setup(struct vfb*s,uint64_t*g){cfgout(s,g,16,4,0xd001);cfgout(s,g,4,2,5);g[0]=3;assert(io(s,g,0xd012,1,false));g[0]=1;assert(io(s,g,0xd008,4,false));}
static void put(unsigned a,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)ram[a+i]=(uint8_t)(v>>(8*i));}
static void submit(void){put(6144,1,2);put(6146,1,2);put(4096,16384,8);put(4104,16,4);put(4108,1,2);put(4110,1,2);put(4112,17408,8);put(4120,512,4);put(4124,3,2);put(4126,2,2);put(4128,19456,8);put(4136,1,4);put(4140,2,2);ram[19456]=99;}
int main(void){struct vfb s;uint64_t g[15]={0};init(&s);
 address(&s,g,0x80000000);assert(io(&s,g,0xcfc,4,true)&&g[0]==0x12378086);
 address(&s,g,0x80001000);assert(io(&s,g,0xcfc,4,true)&&g[0]==0x10011af4);
 g[0]=0xaabbccdd00000000ULL;assert(io(&s,g,0xcfe,2,true)&&g[0]==0xaabbccdd00001001ULL);
 address(&s,g,0x80001800);assert(io(&s,g,0xcfc,4,true)&&g[0]==UINT32_MAX);
 address(&s,g,0x1000);assert(io(&s,g,0xcfc,4,true)&&g[0]==UINT32_MAX);
 address(&s,g,0x80000b40);assert(io(&s,g,0xcfc,4,true)&&g[0]==1);g[0]=0xb001;assert(io(&s,g,0xcfc,4,false));
 address(&s,g,0x80000b80);g[0]=1;assert(io(&s,g,0xcfc,1,false));assert(io(&s,g,0xb008,4,true)&&g[0]==0);
 cfgout(&s,g,16,4,UINT32_MAX);assert(io(&s,g,0xcfc,4,true)&&g[0]==0xffffffc1u);setup(&s,g);submit();g[0]=0;assert(io(&s,g,0xd010,2,false)&&ram[19456]==0&&ram[8194]==1&&pubs==1);
 /* Single latch still selects PIIX, no independent shadow CF8 state. */
 address(&s,g,0x80000b40);assert(io(&s,g,0xcfc,4,true)&&g[0]==0xb001);
 init(&s);setup(&s,g);submit();g[0]=0;commit_fail=true;unsigned before=commits;assert(!io(&s,g,0xd010,2,false)&&s.failed&&s.disk.failed&&commits==before+1);assert(ram[19456]==0&&ram[8194]==1&&pubs==1);commit_fail=false;before=commits;assert(!io(&s,g,0xd010,2,false)&&commits==before&&pubs==1);
 init(&s);cfgout(&s,g,16,4,0xd001);address(&s,g,0x80001010);g[0]=0xb001;assert(!io(&s,g,0xcfc,4,false)&&s.failed&&s.disk.bar==0xd000);
 init(&s);cfgout(&s,g,16,4,0xdfc1);cfgout(&s,g,4,2,1);assert(io(&s,g,0xdfc0,4,true)&&g[0]==32);cfgout(&s,g,16,4,0xd041);assert(io(&s,g,0xd040,4,true)&&g[0]==32);assert(!io(&s,g,0xdfc0,4,true));
 init(&s);cfgout(&s,g,16,4,0xd001);g[0]=77;assert(!io(&s,g,0xd000,4,true)&&g[0]==77); /* disabled decode */
 init(&s);setup(&s,g);submit();g[0]=0;assert(io(&s,g,0xd010,2,false));g[0]=0;assert(io(&s,g,0xd012,1,false));g[0]=3;assert(io(&s,g,0xd012,1,false));g[0]=1;assert(!io(&s,g,0xd008,4,false)); /* stale ring */
 init(&s);address(&s,g,0x80001010);g[0]=0xd001;commit_fail=true;assert(!io(&s,g,0xcfc,4,false)&&!s.disk.bar);commit_fail=false;
 init(&s);address(&s,g,0x80001000);g[0]=7;assert(!io(&s,g,0xcfd,4,true)&&g[0]==7);
 init(&s);address(&s,g,0x80001000);uint8_t b=0xed;g[3]=0xcfc;unsigned old=commits;assert(!vfb_io(&s,g,0x1000,3,0x0cfc000b,1,1,0,&b,1,true,0,commit)&&commits==old);
 init(&s);s.busy=true;assert(!io(&s,g,0xcf8,4,true)&&s.failed);
 puts("single CF8 firmware board + virtio BAR/terminal commit: host PASS");
}
