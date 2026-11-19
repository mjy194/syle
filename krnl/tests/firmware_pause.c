#define _POSIX_C_SOURCE 200112L
#include "../firmware_pause.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT (FWM_RAM_PAGES+FWM_FLASH_PAGES+FWM_TABLE_PAGES)
#define BASE UINT64_C(0x100000000)
struct heap { uint8_t*bytes; unsigned allocated,live,maps,fail_map; };
static uint64_t alloc(void*x){struct heap*h=x;assert(h->allocated<COUNT);h->live++;return BASE+4096u*h->allocated++;}
static bool release(void*x,uint64_t p){struct heap*h=x;assert(p>=BASE&&h->live);h->live--;return true;}
static void*map(void*x,uint64_t p){
 struct heap*h=x;assert(p>=BASE&&p<BASE+(uint64_t)h->allocated*4096&&!(p&4095));
 return ++h->maps==h->fail_map?NULL:h->bytes+(p-BASE);
}
static bool wb(void*x,uint64_t p){(void)x;return p>=BASE;}
static void put(struct firmware_memory*m,uint64_t a,uint64_t v){assert(fwm_write_ram(m,a,&v,8));}
struct field {uint32_t id;uint64_t value;};
struct vm {
 struct field fields[20];
 unsigned reads,writes,fail_read,fail_write;
 uint32_t written[3];uint64_t values[3];
 bool reenter;struct fpa_state*state;struct firmware_memory*memory;
};
static uint64_t*value(struct vm*v,uint32_t field){
 for(unsigned i=0;i<20;i++)if(v->fields[i].id==field)return &v->fields[i].value;
 assert(false);return NULL;
}
static bool rd(void*x,uint32_t field,uint64_t*out){struct vm*v=x;if(++v->reads==v->fail_read)return false;*out=*value(v,field);return true;}
static bool wr(void*x,uint32_t field,uint64_t val){
 struct vm*v=x;assert(v->writes<3);assert(field==0x681e||field==0x6820||field==0x4824);
 v->written[v->writes]=field;v->values[v->writes++]=val;*value(v,field)=val;
 if(v->reenter)assert(fpa_handle(v->state,v->memory,true,v,rd,wr,NULL)==FAT_FATAL);
 return v->writes!=v->fail_write; /* Includes mutation before failure. */
}
static struct vm initial(void){return (struct vm){.fields={
 {0x4402,40},{0x681e,0x7000},{0x440c,2},{0x6820,0x10206},{0x4824,0},{0x6822,0},
 {0x681a,0x400},{0x4016,0},{0x4408,0},{0x4002,0xc0000000},{0x401e,0xc2},{0x4012,0xc204},
 {0x2802,0},{0x6800,0x80010033},{0x6802,0x1000},{0x6804,0x2668},{0x2806,0x500},
 {0x6808,0},{0x4816,0xa09b},{0x0802,8}}};}
static void rejected(struct firmware_memory*m,struct vm*v){
 struct fpa_state s={0},old=s;struct field before[20];memcpy(before,v->fields,sizeof before);
 assert(fpa_handle(&s,m,true,v,rd,wr,NULL)==FAT_REJECTED&&!v->writes);
 assert(!memcmp(&s,&old,sizeof s)&&!memcmp(before,v->fields,sizeof before));
}
int main(void){
 struct heap h={0};struct firmware_memory*m=calloc(1,sizeof*m);assert(m);
 size_t total=(size_t)COUNT*4096;assert(!posix_memalign((void**)&h.bytes,4096,total));
 uint8_t*code=calloc(1,3653632),*vars=calloc(1,540672),*before=malloc(total);assert(code&&vars&&before);
 struct sg_memory_ops ops={&h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,3653632,vars,540672,36));
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0xe3);
 const uint8_t pause[]={0xf3,0x90};assert(fwm_write_ram(m,0x7000,pause,2));
 memcpy(before,h.bytes,total);
 struct vm v=initial();struct fpa_state s={0};struct fpa_trace t={0};
 assert(fpa_handle(&s,m,true,&v,rd,wr,&t)==FAT_COMMITTED);
 assert(s.completed==1&&!s.failed&&!s.busy&&v.writes==2&&v.written[0]==0x6820&&v.values[0]==0x206&&v.written[1]==0x681e&&v.values[1]==0x7002);
 assert(t.rip==0x7000&&t.length==2&&t.bytes[0]==0xf3&&t.bytes[1]==0x90&&t.available==15);
 assert(!memcmp(before,h.bytes,total));
 v=initial();*value(&v,0x6820)=0x206;
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_COMMITTED&&s.completed==2&&v.writes==1&&v.written[0]==0x681e);
 /* Invalid vectoring payload is ignored only when its valid bit is clear. */
 v=initial();*value(&v,0x4408)=0x1234;
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_COMMITTED&&s.completed==3);
 for(unsigned shadow=1;shadow<=2;shadow++){
  v=initial();*value(&v,0x4824)=shadow;
  assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_COMMITTED&&v.writes==3);
  assert(v.written[1]==0x4824&&v.values[1]==0&&v.written[2]==0x681e);
 }
 v=initial();*value(&v,0x4824)=1;*value(&v,0x6820)=2;rejected(m,&v);
 for(unsigned i=1;i<=20;i++){v=initial();v.fail_read=i;rejected(m,&v);assert(v.reads==i);}
 const struct field invalid[]={
 {0x4402,0x80000028},{0x4402,52},{0x440c,0},{0x440c,1},{0x440c,3},{0x440c,UINT64_C(0x100000002)},
 {0x6820,0x204},{0x6820,0x10306},{0x6820,0x30206},{0x6820,UINT64_MAX},
 {0x4824,3},{0x4824,8},{0x6822,1},{0x681a,0x401},{0x4016,0x80000020},
 {0x4408,0x80000320},{0x4408,UINT64_C(1)<<32},{0x4002,0x80000000},{0x4002,0x40000000},
 {0x4002,UINT64_C(0x1c0000000)},{0x401e,0x42},{0x401e,0x4c2},{0x4012,0xc004},
 {0x4012,UINT64_C(0x10000c204)},{0x2802,1},{0x6800,0x10033},{0x6804,0x3668},
 {0x2806,0xd00},{0x6808,1},{0x4816,0xc09b},{0x4816,UINT64_C(0x10000a09b)},
 {0x0802,0},{0x0802,0xb},{0x0802,0x10008},{0x681e,UINT64_MAX-1},
 {0x681e,UINT64_C(0x7ffffffffffe)},{0x681e,UINT64_C(0x800000000000)}};
 for(unsigned i=0;i<sizeof invalid/sizeof*invalid;i++){v=initial();*value(&v,invalid[i].id)=invalid[i].value;rejected(m,&v);}
 for(unsigned fail=1;fail<=2;fail++){
  s=(struct fpa_state){0};v=initial();v.fail_write=fail;
  assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&!s.busy&&!s.completed&&v.writes==fail);
  assert(*value(&v,0x6820)==0x206);if(fail==2)assert(*value(&v,0x681e)==0x7002);
  unsigned reads=v.reads,writes=v.writes;
  assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_REJECTED&&v.reads==reads&&v.writes==writes);
 }
 s=(struct fpa_state){0};v=initial();*value(&v,0x6820)=0x206;v.fail_write=1;
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&v.writes==1&&*value(&v,0x681e)==0x7002);
 s=(struct fpa_state){0};v=initial();v.reenter=true;v.state=&s;v.memory=m;
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&!s.completed&&v.writes==1);
 s=(struct fpa_state){.completed=UINT64_MAX};v=initial();
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_REJECTED&&!v.reads&&!v.writes);
 s=(struct fpa_state){0};assert(fpa_handle(&s,m,false,&v,rd,wr,NULL)==FAT_REJECTED&&!v.reads);
 assert(fpa_handle(&s,m,true,&v,NULL,wr,NULL)==FAT_REJECTED);
 assert(fpa_handle(&s,m,true,&v,rd,NULL,NULL)==FAT_REJECTED);
 assert(fpa_handle(NULL,m,true,&v,rd,wr,NULL)==FAT_REJECTED);
 /* Real instruction fetch rejects wrong opcodes/prefixes and missing bytes. */
 uint8_t*insn=h.bytes+(m->ram[7]-BASE);
 for(unsigned i=0;i<256;i++)if(i!=0xf3){insn[0]=(uint8_t)i;v=initial();rejected(m,&v);}
 insn[0]=0xf3;
 for(unsigned i=0;i<256;i++)if(i!=0x90){insn[1]=(uint8_t)i;v=initial();rejected(m,&v);}
 insn[1]=0x90;
 put(m,0x1000,0x2003);v=initial();rejected(m,&v);put(m,0x1000,0x2023);
 put(m,0x3000,0);v=initial();rejected(m,&v);put(m,0x3000,0xe3);
 m->quarantined=true;v=initial();rejected(m,&v);m->quarantined=false;
 for(unsigned fail=1;fail<=8;fail++){h.fail_map=h.maps+fail;v=initial();rejected(m,&v);h.fail_map=0;}
 /* 4KiB leaf and cross-page fetch; no need to read past the two-byte opcode. */
 put(m,0x3000,0x4023);put(m,0x4000+7*8,0x7023);v=initial();s=(struct fpa_state){0};
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_COMMITTED);
 assert(fwm_write_ram(m,0x7fff,pause,2));v=initial();*value(&v,0x681e)=0x7fff;
 rejected(m,&v); /* Second page absent: only F3 is readable. */
 put(m,0x4000+8*8,0x8023);v=initial();*value(&v,0x681e)=0x7fff;
 assert(fpa_handle(&s,m,true,&v,rd,wr,NULL)==FAT_COMMITTED&&*value(&v,0x681e)==0x8001);
 uint8_t zero[2]={0};assert(fwm_write_ram(m,0x7fff,zero,2));
 put(m,0x4000+7*8,0);put(m,0x4000+8*8,0);put(m,0x3000,0xe3);
 assert(!memcmp(before,h.bytes,total));
 assert(fwm_release(m)&&!h.live);free(before);free(code);free(vars);free(h.bytes);free(m);
 puts("firmware PAUSE: owned F3 90, VMCS/control/mode gates, RF/RIP, no memory changes, partial failure stop PASS");
}
