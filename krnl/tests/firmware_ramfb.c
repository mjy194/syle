#include "../firmware_ramfb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t memory[4*1024*1024];static unsigned writes;static bool fail_write;
static bool owned(void*x,uint64_t a,uint64_t n){(void)x;return n&&a<sizeof memory&&n<=sizeof memory-a;}
static bool readmem(void*x,uint64_t a,void*p,unsigned n){if(!owned(x,a,n))return false;memcpy(p,memory+a,n);return true;}
static bool writemem(void*x,uint64_t a,const void*p,unsigned n){writes++;if(fail_write||!owned(x,a,n))return false;memcpy(memory+a,p,n);return true;}
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-i*8));}
static void put64(uint8_t*p,uint64_t v){put32(p,(uint32_t)(v>>32));put32(p+4,(uint32_t)v);}
static void descriptor(uint32_t ctl,uint32_t n,uint64_t a){put32(memory+0x1000,ctl);put32(memory+0x1004,n);put64(memory+0x1008,a);}
static bool trigger(struct frfb_state*s,struct ffc_state*f){uint64_t a=__builtin_bswap32(0x1000);return frfb_io(s,f,0x518,4,false,&a,0,readmem,writemem,owned);}
int main(void){
 struct ffc_state f={0};struct frfb_state s={0};assert(ffc_init(&f,FWM_RAM_SIZE,1));uint64_t a=1;
 assert(ffc_io(&f,0x510,2,false,&a));descriptor(2,4,0x2000);assert(trigger(&s,&f)&&memory[0x2000]==3&&s.transfers==1);
 a=0x19;assert(ffc_io(&f,0x510,2,false,&a));descriptor(2,4,0x2000);assert(trigger(&s,&f)&&memory[0x2003]==1);
 descriptor(2,64,0x2000);assert(trigger(&s,&f)&&memory[0x2003]==28&&!memcmp(memory+0x2008,"etc/ramfb",9));
 uint8_t*p=memory+0x3000;put64(p,0x10000);put32(p+8,0x34325258);put32(p+12,0);put32(p+16,800);put32(p+20,600);put32(p+24,3200);
 descriptor((0x20u<<16)|8|16,28,0x3000);assert(trigger(&s,&f)&&s.active&&s.config.address==0x10000&&s.config.width==800&&s.configurations==1);
 struct frfb_state original=s;unsigned before;
 const uint32_t bad[][2]={{16,27},{18,28},{1,28},{32,28},{2,65}};
 for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){f.selector=0x20;f.cursor=0;descriptor(bad[i][0],bad[i][1],0x3000);before=writes;struct frfb_state next=s;assert(!trigger(&next,&f)&&writes==before&&!memcmp(&next,&original,sizeof s));}
 f.selector=0x20;f.cursor=0;put64(p,UINT64_MAX-16);descriptor(16,28,0x3000);before=writes;assert(!trigger(&s,&f)&&writes==before);
 descriptor(2,4,0x1000);assert(!trigger(&s,&f));
 descriptor(2,4,0x2000);f.selector=0;f.cursor=0;fail_write=true;assert(!trigger(&s,&f));fail_write=false;
 a=0;assert(frfb_io(&s,&f,0x514,4,true,&a,0,readmem,writemem,owned)&&a==0x554d4551);
 assert(!frfb_io(&s,&f,0x514,2,true,&a,0,readmem,writemem,owned));
 puts("ramfb DMA: feature/directory/read/select/write config, BE, completion, bounds, overlap, invalid requests and partial write failure PASS");
}
