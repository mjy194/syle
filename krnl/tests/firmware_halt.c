#define _POSIX_C_SOURCE 200112L
#include "../firmware_halt.h"
#include "../firmware_timer_irq_delivery.h"
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
 struct field fields[24];unsigned reads,writes,fail_read,fail_write;
 uint32_t written[4];uint64_t values[4];
 bool reenter;struct fhl_state*state;struct firmware_memory*memory;
};
static uint64_t*value(struct vm*v,uint32_t field){
 for(unsigned i=0;i<24;i++)if(v->fields[i].id==field)return &v->fields[i].value;
 assert(false);return NULL;
}
static bool rd(void*x,uint32_t field,uint64_t*out){struct vm*v=x;if(++v->reads==v->fail_read)return false;*out=*value(v,field);return true;}
static bool wr(void*x,uint32_t field,uint64_t val){
 struct vm*v=x;assert(v->writes<4);
 assert(field==0x681e||field==0x6820||field==0x4824||field==0x4826);
 v->written[v->writes]=field;v->values[v->writes++]=val;*value(v,field)=val;
 if(v->reenter)assert(fhl_handle(v->state,v->memory,true,true,v,rd,wr,NULL)==FAT_FATAL);
 return v->writes!=v->fail_write; /* Mutation before failure is deliberate. */
}
static struct vm initial(void){return (struct vm){.fields={
 {0x4402,12},{0x681e,0x7000},{0x440c,1},{0x6820,0x10206},{0x4824,1},{0x6822,0},
 {0x681a,0x400},{0x4016,0},{0x4408,0},{0x4002,0xc0000080},{0x401e,0xc2},{0x4012,0xc204},
 {0x2802,0},{0x6800,0x80010033},{0x6802,0x1000},{0x6804,0x2668},{0x2806,0x500},
 {0x6808,0},{0x4816,0xa09b},{0x0802,8},{0x4826,0},{0x4818,0xc093},{0x4000,0x49},{0x4404,0}}};}
static void rejected(struct firmware_memory*m,struct vm*v){
 struct fhl_state s={0},before=s;struct field fields[24];memcpy(fields,v->fields,sizeof fields);
 enum fat_result result=fhl_handle(&s,m,true,true,v,rd,wr,NULL);
 if(result!=FAT_REJECTED||v->writes){
  fprintf(stderr,"unexpected admission result=%d reads=%u writes=%u\n",result,v->reads,v->writes);
  for(unsigned i=0;i<24;i++)fprintf(stderr,"%x=%llx ",v->fields[i].id,(unsigned long long)v->fields[i].value);
  fputc('\n',stderr);
 }
 assert(result==FAT_REJECTED&&!v->writes);
 assert(!memcmp(&s,&before,sizeof s)&&!memcmp(fields,v->fields,sizeof fields));
}
static struct fhl_state halted(struct firmware_memory*m,struct vm*v){
 *v=initial();struct fhl_state s={0};
 assert(fhl_handle(&s,m,true,true,v,rd,wr,NULL)==FAT_COMMITTED);
 v->reads=v->writes=0;*value(v,0x4402)=52;*value(v,0x4826)=0;
 return s;
}
static bool irq_write(void*c,uint32_t field,uint64_t v){
 struct vm*m=c;
 if(field==0x4016)*value(m,field)=v;
 else assert((field==0x4018||field==0x401a)&&!v);
 return true;
}
static void irq_cycle(struct firmware_memory*m){
 struct vm v;struct fhl_state h=halted(m,&v);struct ftis_session s={0};
 assert(ftis_init(&s,7,0x1234,0,UINT64_MAX));
 for(unsigned n=1;n<=10;n++){
  assert(ftis_sample(&s,7,0x1234,n*90000));assert(!s.owner.pending);
  assert(fhl_after(&h,true,true,false,false,&v,rd,wr,NULL)==FAT_COMMITTED);
  assert(h.halted&&!h.wakeups&&*value(&v,0x681e)==0x7001&&*value(&v,0x4826)==1);
  v.writes=0;*value(&v,0x4826)=0; /* Host timer wakes hardware, not guest. */
 }
 assert(ftis_sample(&s,7,0x1234,1000000)&&s.owner.pending);
 *value(&v,0x4826)=1;struct lei_snapshot g={.rflags=0x206,.activity=1};uint64_t attempt;
 assert(ftis_before(&s,7,0x1234,&g,&v,irq_write,&attempt)==FTIS_STAGED);
 assert(h.halted&&!h.wakeups&&*value(&v,0x4016)==0x80000020);
 struct fine_outcome out={.evidence={.observed={7,0x1234,attempt},.staged_info=0x80000020,
 .native_status=1,.reason=48,.run_reported_exit=true,.host_restored=true,.profile_admitted=true},
 .run_result=LVS_EXIT,.status_sentinel_set=true,.exit_fields_read=true};
 *value(&v,0x4402)=48;*value(&v,0x4826)=0;*value(&v,0x681e)=0x9000;*value(&v,0x6820)=0x46;
 assert(ftid_after(&s,&out,&v,irq_write)==FTIS_ACCOUNTED&&s.owner.in_service);
 assert(fhl_after(&h,true,true,true,true,&v,rd,wr,NULL)==FAT_COMMITTED&&!h.halted&&h.wakeups==1&&!v.writes);
 assert(ftis_eoi(&s,7,0x1234)&&!s.owner.in_service);
 v=initial();assert(fhl_handle(&h,m,true,true,&v,rd,wr,NULL)==FAT_COMMITTED&&h.completed==2);
}
int main(void){
 struct heap h={0};struct firmware_memory*m=calloc(1,sizeof*m);assert(m);
 size_t total=(size_t)COUNT*4096;assert(!posix_memalign((void**)&h.bytes,4096,total));
 uint8_t*code=calloc(1,3653632),*vars=calloc(1,540672),*before=malloc(total);assert(code&&vars&&before);
 struct sg_memory_ops ops={&h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,3653632,vars,540672,36));
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0xe3);
 uint8_t opcode=0xf4;assert(fwm_write_ram(m,0x7000,&opcode,1));memcpy(before,h.bytes,total);
 struct vm v;struct fhl_state s;struct fhl_trace trace;
 for(unsigned block=0;block<=2;block++)for(unsigned rf=0;rf<2;rf++){
  v=initial();s=(struct fhl_state){0};*value(&v,0x4824)=block;*value(&v,0x6820)=0x206|(rf?0x10000:0);
  assert(fhl_handle(&s,m,true,true,&v,rd,wr,&trace)==FAT_COMMITTED);
  assert(s.halted&&s.completed==1&&!s.failed&&!s.busy&&s.resume_rip==0x7001&&s.halt_flags==0x206);
  assert(v.writes==2+rf+(block!=0)&&v.written[v.writes-2]==0x681e&&v.written[v.writes-1]==0x4826);
  assert(*value(&v,0x4826)==1&&!*value(&v,0x4824)&&*value(&v,0x6820)==0x206);
  assert(trace.opcode==0xf4&&trace.available==15);
 }
 for(unsigned n=1;n<=23;n++){v=initial();v.fail_read=n;rejected(m,&v);assert(v.reads==n);}
 const struct field invalid[]={
 {0x4402,0x8000000c},{0x4402,52},{0x440c,0},{0x440c,2},{0x440c,UINT64_C(0x100000001)},
 {0x6820,0x204},{0x6820,0x10306},{0x6820,0x6},{0x6820,UINT64_MAX},{0x4824,3},{0x4824,8},
 {0x6822,1},{0x681a,0x401},{0x4016,0x80000020},{0x4408,0x80000020},{0x4408,UINT64_C(1)<<32},
 {0x4002,0x80000000},{0x4002,0x80},{0x4002,0xc0000084},{0x4002,UINT64_C(0x180000080)},
 {0x401e,0x42},{0x401e,0x4c2},{0x4012,0xc004},{0x4012,UINT64_C(0x10000c204)},{0x2802,1},
 {0x6800,0x10033},{0x6804,0x3668},{0x2806,0xd00},{0x6808,1},{0x4816,0xc09b},
 {0x4816,UINT64_C(0x10000a09b)},{0x0802,0xb},{0x0802,0x10008},{0x4826,1},{0x4826,2},
 {0x4818,0xc0f3},{0x4818,UINT64_C(0x10000c093)},{0x4000,0x9},{0x4000,UINT64_C(0x100000049)},
 {0x681e,UINT64_MAX},{0x681e,UINT64_C(0x7fffffffffff)},{0x681e,UINT64_C(0x800000000000)}};
 for(unsigned i=0;i<sizeof invalid/sizeof*invalid;i++){v=initial();*value(&v,invalid[i].id)=invalid[i].value;rejected(m,&v);}
 for(unsigned n=1;n<=4;n++){
  v=initial();s=(struct fhl_state){0};v.fail_write=n;
  assert(fhl_handle(&s,m,true,true,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&!s.completed&&!s.halted&&v.writes==n);
  unsigned reads=v.reads;assert(fhl_handle(&s,m,true,true,&v,rd,wr,NULL)==FAT_REJECTED&&v.reads==reads);
 }
 s=(struct fhl_state){0};v=initial();v.reenter=true;v.state=&s;v.memory=m;
 assert(fhl_handle(&s,m,true,true,&v,rd,wr,NULL)==FAT_FATAL&&v.writes==1&&!s.completed);
 v=initial();s=(struct fhl_state){0};assert(fhl_handle(&s,m,true,false,&v,rd,wr,NULL)==FAT_REJECTED&&!v.reads);
 assert(fhl_handle(&s,m,false,true,&v,rd,wr,NULL)==FAT_REJECTED&&!v.reads);
 s.completed=UINT64_MAX;assert(fhl_handle(&s,m,true,true,&v,rd,wr,NULL)==FAT_REJECTED&&!v.reads);
 uint8_t*insn=h.bytes+(m->ram[7]-BASE);
 for(unsigned i=0;i<256;i++)if(i!=0xf4){*insn=(uint8_t)i;v=initial();rejected(m,&v);}
 *insn=0xf4;
 put(m,0x1000,0x2003);v=initial();rejected(m,&v);put(m,0x1000,0x2023);
 m->quarantined=true;v=initial();rejected(m,&v);m->quarantined=false;
 /* Once F4 was fetched, failure fetching later bytes need not reject HLT. */
 for(unsigned n=1;n<=8;n++){
  h.fail_map=h.maps+n;v=initial();s=(struct fhl_state){0};
  enum fat_result fetched=fhl_handle(&s,m,true,true,&v,rd,wr,&trace);
  assert((fetched==FAT_REJECTED&&!v.writes&&!trace.available)||
         (fetched==FAT_COMMITTED&&trace.available>=1&&trace.opcode==0xf4&&s.halted));
  h.fail_map=0;
 }
 put(m,0x3000,0x4023);put(m,0x4000+7*8,0x7023);assert(fwm_write_ram(m,0x7fff,&opcode,1));
 v=initial();*value(&v,0x681e)=0x7fff;s=(struct fhl_state){0};
 assert(fhl_handle(&s,m,true,true,&v,rd,wr,&trace)==FAT_COMMITTED&&trace.available==1&&s.resume_rip==0x8000);
 uint8_t zero=0;assert(fwm_write_ram(m,0x7fff,&zero,1));put(m,0x4000+7*8,0);put(m,0x3000,0xe3);
 /* Timer exits must never be mistaken for guest interrupt delivery. */
 s=halted(m,&v);assert(fhl_after(&s,true,true,false,false,&v,rd,wr,&trace)==FAT_COMMITTED);
 assert(trace.activity==0&&s.halted&&s.idle_exits==1&&!s.wakeups&&v.writes==1&&v.written[0]==0x4826);
 v.writes=0;assert(fhl_after(&s,true,true,false,false,&v,rd,wr,NULL)==FAT_COMMITTED&&!v.writes&&s.idle_exits==2);
 for(unsigned n=1;n<=10;n++){
  s=halted(m,&v);v.fail_read=n;assert(fhl_after(&s,true,true,false,false,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&!v.writes);
 }
 const struct field bad_exit[]={{0x4402,12},{0x4402,0x80000034},{0x681e,0x7002},{0x6820,0x246},
 {0x4824,1},{0x4826,2},{0x6822,1},{0x681a,0x401},{0x4016,0x80000020},{0x4408,0x80000020},{0x4404,0x80000020}};
 for(unsigned n=0;n<sizeof bad_exit/sizeof*bad_exit;n++){
  s=halted(m,&v);*value(&v,bad_exit[n].id)=bad_exit[n].value;
  assert(fhl_after(&s,true,true,false,false,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&!v.writes);
 }
 s=halted(m,&v);v.fail_write=1;assert(fhl_after(&s,true,true,false,false,&v,rd,wr,NULL)==FAT_FATAL&&s.failed&&*value(&v,0x4826)==1);
 s=halted(m,&v);v.reenter=true;v.state=&s;v.memory=m;assert(fhl_after(&s,true,true,false,false,&v,rd,wr,NULL)==FAT_FATAL&&s.failed);
 s=halted(m,&v);assert(fhl_after(&s,true,false,false,false,&v,rd,wr,NULL)==FAT_FATAL);
 s=halted(m,&v);assert(fhl_after(&s,true,true,true,false,&v,rd,wr,NULL)==FAT_FATAL&&!s.wakeups);
 s=halted(m,&v);assert(fhl_after(&s,true,true,false,true,&v,rd,wr,NULL)==FAT_FATAL&&!s.wakeups);
 s=halted(m,&v);*value(&v,0x4826)=1;assert(fhl_after(&s,true,true,true,true,&v,rd,wr,NULL)==FAT_FATAL&&!s.wakeups);
 s=halted(m,&v);*value(&v,0x4402)=7;assert(fhl_after(&s,true,true,true,true,&v,rd,wr,NULL)==FAT_FATAL);
 irq_cycle(m);
 assert(!memcmp(before,h.bytes,total));assert(fwm_release(m)&&!h.live);
 free(before);free(code);free(vars);free(h.bytes);free(m);
 puts("firmware HLT: owned F4, completion/failure gates, timer re-halt, real IRQ receipt/wake/EOI cycle PASS");
}
