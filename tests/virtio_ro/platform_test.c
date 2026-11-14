#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t ram[32768],other[32768],disk[1024];
static struct vrp_region region(void){return (struct vrp_region){0,sizeof(ram),ram,42,true,true,true};}
static void init(struct vrp *s){struct vrp_region r=region();struct vrp_ports p={0xc000,0xc040};memset(ram,0,sizeof(ram));assert(vrp_init(s,16,42,&r,1,&p,1,disk,sizeof(disk)));}
static int cfg(struct vrp *s,unsigned off,unsigned w,uint32_t v){return vrp_config(s,16,off,w,true,&v);}
static int out(struct vrp *s,unsigned off,unsigned w,uint32_t v){return vrp_io(s,(uint16_t)(s->dev.bar+off),w,true,&v);}
static void put(unsigned a,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)ram[a+i]=(uint8_t)(v>>(8*i));}
static void ready(struct vrp *s){assert(cfg(s,16,4,0xd001)==1);assert(cfg(s,4,2,5)==1);assert(out(s,18,1,3)==1);assert(out(s,8,4,1)==1);}
static void submit(void){put(6144,1,2);put(6146,1,2);put(4096,16384,8);put(4104,16,4);put(4108,1,2);put(4110,1,2);put(4112,17408,8);put(4120,512,4);put(4124,3,2);put(4126,2,2);put(4128,19456,8);put(4136,1,4);put(4140,2,2);ram[19456]=99;}
int main(void){struct vrp s;uint32_t v=0;struct vrp_region r[2];
 init(&s);assert(vrp_config(&s,17,0,4,false,&v)==0&&!s.dev.failed);assert(vrp_io(&s,0x3f8,1,false,&v)==0&&!s.dev.failed);
 ready(&s);submit();assert(out(&s,16,2,0)==1&&ram[19456]==0&&ram[8194]==1&&s.barriers==1);vrp_abort(&s);unsigned barriers=s.barriers;assert(out(&s,16,2,0)==-1&&s.barriers==barriers);
 init(&s);assert(cfg(&s,16,4,0xd001)==1);assert(cfg(&s,16,4,0xc001)==-1&&s.dev.bar==0xd000&&!s.active);
 init(&s);assert(cfg(&s,16,4,UINT32_MAX)==1);assert(vrp_io(&s,0xd000,4,false,&v)==0);assert(cfg(&s,16,4,0xe001)==1);assert(cfg(&s,4,2,1)==1);assert(vrp_io(&s,0xd000,4,false,&v)==0);assert(vrp_io(&s,0xe000,4,false,&v)==1&&v==32);assert(cfg(&s,4,2,0)==1);assert(vrp_io(&s,0xe000,4,false,&v)==0&&!s.dev.failed);
 init(&s);put(6146,1,2);assert(cfg(&s,16,4,0xd001)==1);assert(cfg(&s,4,2,5)==1);assert(out(&s,18,1,3)==1);assert(out(&s,8,4,1)==-1&&!s.active);
 init(&s);ready(&s);submit();assert(out(&s,16,2,0)==1);assert(out(&s,18,1,0)==1);assert(out(&s,18,1,3)==1);assert(out(&s,8,4,1)==-1); /* old published queue */
 r[0]=region();r[0].owner=9;assert(!vrp_init(&s,16,42,r,1,0,0,disk,sizeof(disk))&&!s.active);
 r[0]=region();r[0].ram=false;assert(!vrp_init(&s,16,42,r,1,0,0,disk,sizeof(disk)));
 r[0]=region();r[1]=region();r[1].gpa=65536;assert(!vrp_init(&s,16,42,r,2,0,0,disk,sizeof(disk))); /* host alias */
 r[0]=region();r[0].host=disk;r[0].size=sizeof(disk);assert(!vrp_init(&s,16,42,r,1,0,0,disk,sizeof(disk)));
 r[0]=region();r[0].host=(uint8_t*)&s;r[0].size=sizeof(s);assert(!vrp_init(&s,16,42,r,1,0,0,disk,sizeof(disk)));
 r[0]=region();r[0].write=false;memset(ram,0,sizeof(ram));assert(vrp_init(&s,16,42,r,1,0,0,disk,sizeof(disk)));assert(cfg(&s,16,4,0xd001)==1);assert(cfg(&s,4,2,5)==1);assert(out(&s,18,1,3)==1);assert(out(&s,8,4,1)==-1);
 init(&s);assert(cfg(&s,16,4,0xbfc1)==1);assert(cfg(&s,16,4,0xc041)==1);
 init(&s);assert(cfg(&s,16,4,0xffc1)==1);assert(cfg(&s,4,2,1)==1);assert(vrp_io(&s,0xffc0,4,false,&v)==1);
 r[0]=region();struct vrp_ports last={65535,65536};assert(vrp_init(&s,16,42,r,1,&last,1,disk,sizeof(disk)));assert(cfg(&s,16,4,0xffc1)==-1);
 r[0]=region();r[1]=region();r[1].host=other;assert(!vrp_init(&s,16,42,r,2,0,0,disk,sizeof(disk)));
 r[0]=region();r[0].size=4096;r[1]=region();r[1].gpa=4096;r[1].host=other;r[1].size=4096;assert(vrp_init(&s,16,42,r,2,0,0,disk,sizeof(disk)));assert(!s.dev.io.admit(&s,4095,2,true));
 init(&s);uint8_t byte=7;ram[0]=42;vrp_abort(&s);assert(!s.dev.io.admit(&s,0,1,true));assert(!s.dev.io.read(&s,0,&byte,1)&&byte==7);assert(!s.dev.io.write(&s,0,&byte,1)&&ram[0]==42);
 puts("virtio platform adapter host pilot: PASS (trusted allocation records only)");return 0;
}
