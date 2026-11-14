#include "ramfb_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-i*8));}
static void put64(uint8_t*p,uint64_t v){put32(p,(uint32_t)(v>>32));put32(p+4,(uint32_t)v);}
static void config(uint8_t*p,uint64_t addr,uint32_t w,uint32_t h,uint32_t stride){memset(p,0,28);put64(p,addr);put32(p+8,0x34325258);put32(p+16,w);put32(p+20,h);put32(p+24,stride);}
static void no(uint8_t*p,size_t n,uint64_t b,uint64_t s){struct ramfb_config out={0},old;memset(&out,0xa5,sizeof out);memcpy(&old,&out,sizeof old);assert(!ramfb_parse(p,n,b,s,&out));assert(!memcmp(&out,&old,sizeof out));}
int main(void){uint8_t p[29],saved[29];struct ramfb_config out;
 unsigned modes[][2]={{640,480},{800,600},{1024,768}};
 for(unsigned i=0;i<3;i++){uint32_t w=modes[i][0],h=modes[i][1];config(p,0x100000,w,h,w*4);memcpy(saved,p,28);assert(ramfb_parse(p,28,0,128*1024*1024,&out));assert(out.address==0x100000&&out.span==(uint64_t)w*h*4&&out.width==w&&out.height==h&&out.stride==w*4);assert(!memcmp(saved,p,28));}
 config(p,0x123456789ULL,800,600,3200);assert(ramfb_parse(p,28,0x123456789ULL,1920000,&out)&&out.address==0x123456789ULL);no(p,28,0x123456789ULL,1919999);
 config(p,4096,16,16,128);assert(ramfb_parse(p,28,4096,1984,&out)&&out.span==1984);no(p,28,4096,1983);
 for(size_t n=0;n<28;n++){no(p,n,0,1<<24);}no(p,29,0,1<<24);no(0,28,0,1<<24);assert(!ramfb_parse(p,28,0,1<<24,0));
 uint32_t bad[]={0,1,15,UINT32_MAX,0x40000000};
 for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){config(p,0,bad[i],16,UINT32_MAX);no(p,28,0,UINT64_MAX);config(p,0,16,bad[i],UINT32_MAX);no(p,28,0,UINT64_MAX);}
 config(p,0,1025,16,4100);no(p,28,0,1<<24);config(p,0,16,769,64);no(p,28,0,1<<24);
 config(p,0,800,600,0);no(p,28,0,1<<24);put32(p+24,3199);no(p,28,0,1<<24);
 config(p,UINT64_MAX-30,16,16,64);no(p,28,0,UINT64_MAX);
 config(p,0,16,16,UINT32_MAX);no(p,28,0,128*1024*1024);assert(ramfb_parse(p,28,0,UINT64_MAX,&out)&&out.span==UINT64_C(15)*UINT32_MAX+64);
 config(p,4096,16,16,64);no(p,28,4097,4096);no(p,28,0,4096);no(p,28,0,0);no(p,28,UINT64_MAX,1);
 put32(p+8,0x58523234);no(p,28,0,1<<24);put32(p+8,0x34325258);put32(p+12,1);no(p,28,0,1<<24);
 puts("ramfb host contract PASS: BE28, modes, exact extent, rejection atomicity, overflow/range, immutable input");}
