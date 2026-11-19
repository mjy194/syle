#include "firmware_pause.h"

static bool canonical(uint64_t a){return (a>>47)==0||(a>>47)==0x1ffff;}
static enum fat_result fatal(struct fpa_state*s){s->failed=true;s->busy=false;return FAT_FATAL;}
enum fat_result fpa_handle(struct fpa_state*s,struct firmware_memory*memory,bool exclusive,
 void*ctx,bool(*read)(void*,uint32_t,uint64_t*),
 bool(*write)(void*,uint32_t,uint64_t),struct fpa_trace*trace){
 if(!exclusive)return FAT_REJECTED;
 if(s&&s->busy)return fatal(s);
 if(!s||s->failed||s->completed==UINT64_MAX||!memory||!read||!write)return FAT_REJECTED;
 /* Width checks precede narrowing; raw reason includes entry-failure flags. */
 const uint32_t fields[]={0x4402,0x681e,0x440c,0x6820,0x4824,0x6822,
  0x681a,0x4016,0x4408,0x4002,0x401e,0x4012,0x2802,
  0x6800,0x6802,0x6804,0x2806,0x6808,0x4816,0x0802};
 uint64_t v[sizeof fields/sizeof*fields];
 for(unsigned i=0;i<sizeof fields/sizeof*fields;i++)
  if(!read(ctx,fields[i],&v[i]))return FAT_REJECTED;
 struct fpa_trace t={.rip=v[1],.length=v[2],.flags=v[3],.primary=v[9],.secondary=v[10]};
 if(trace)*trace=t;
 if(v[0]!=40||v[2]!=2||!(v[3]&2)||(v[3]&~UINT64_C(0x250ed7))||
 v[4]>2||(v[4]==1&&!(v[3]&0x200))||v[5]||v[6]!=0x400||v[7]||v[8]>UINT32_MAX||(v[8]&UINT64_C(0x80000000))||
 v[9]>UINT32_MAX||(v[9]&UINT64_C(0xc0000000))!=UINT64_C(0xc0000000)||
 v[10]!=0xc2||v[11]>UINT32_MAX||!(v[11]&0x200)||v[12]||
 v[18]>UINT32_MAX||v[19]>UINT16_MAX||
 !canonical(v[1])||v[1]>UINT64_MAX-2||!canonical(v[1]+2))return FAT_REJECTED;
 /* An invalid IDT-vectoring field's payload is architecturally undefined.
  * Only its valid bit denotes an event; no event is injected or acknowledged. */
 struct fmm_memory m={.memory=memory,.cr0=v[13],.cr3=v[14],.cr4=v[15],
  .efer=v[16],.cs_base=v[17],.cs_ar=(uint32_t)v[18],.cs_selector=(uint16_t)v[19]};
 uint8_t bytes[15];t.available=(unsigned)fmm_fetch(&m,t.rip,bytes);
 for(unsigned i=0;i<2&&i<t.available;i++)t.bytes[i]=bytes[i];
 if(trace)*trace=t;
 if(t.available<2||bytes[0]!=0xf3||bytes[1]!=0x90)return FAT_REJECTED;
 s->busy=true;
 if(((t.flags&0x10000)&&(!write(ctx,0x6820,t.flags&~UINT64_C(0x10000))||s->failed||!s->busy))||
 (v[4]&&(!write(ctx,0x4824,0)||s->failed||!s->busy))||
 !write(ctx,0x681e,t.rip+2)||s->failed||!s->busy)return fatal(s);
 s->busy=false;s->completed++;return FAT_COMMITTED;
}
