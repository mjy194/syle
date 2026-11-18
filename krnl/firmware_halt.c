#include "firmware_halt.h"
static bool canonical(uint64_t a){return (a>>47)==0||(a>>47)==0x1ffff;}
static enum fat_result fatal(struct fhl_state*s){s->failed=true;s->busy=false;return FAT_FATAL;}
static bool flags_ok(uint64_t f){return (f&2)&&!(f&~UINT64_C(0x250ed7))&&!(f&0x100);}
static bool store(struct fhl_state*s,void*c,bool(*w)(void*,uint32_t,uint64_t),uint32_t f,uint64_t v){
 return w(c,f,v)&&!s->failed&&s->busy;
}
enum fat_result fhl_handle(struct fhl_state*s,struct firmware_memory*memory,bool exclusive,
 bool supported,void*c,bool(*r)(void*,uint32_t,uint64_t*),
 bool(*w)(void*,uint32_t,uint64_t),struct fhl_trace*trace){
 if(!exclusive)return FAT_REJECTED;
 if(s&&s->busy)return fatal(s);
 if(!s||s->failed||s->halted||s->completed==UINT64_MAX||!memory||!r||!w||!supported)return FAT_REJECTED;
 const uint32_t fields[]={0x4402,0x681e,0x440c,0x6820,0x4824,0x6822,
  0x681a,0x4016,0x4408,0x4002,0x401e,0x4012,0x2802,
  0x6800,0x6802,0x6804,0x2806,0x6808,0x4816,0x0802,
  0x4826,0x4818,0x4000};
 uint64_t v[sizeof fields/sizeof*fields];
 for(unsigned i=0;i<sizeof fields/sizeof*fields;i++)if(!r(c,fields[i],&v[i]))return FAT_REJECTED;
 struct fhl_trace t={.reason=v[0],.rip=v[1],.flags=v[3],.blocking=v[4],.activity=v[20]};
 if(trace)*trace=t;
 if(v[0]!=12||v[2]!=1||!flags_ok(v[3])||!(v[3]&0x200)||v[4]>2||v[5]||
 v[6]!=0x400||v[7]||v[8]>UINT32_MAX||(v[8]&0x80000000)||
 v[9]>UINT32_MAX||(v[9]&UINT64_C(0x80000080))!=UINT64_C(0x80000080)||(v[9]&4)||
 v[10]!=0xc2||v[11]>UINT32_MAX||!(v[11]&0x200)||v[12]||
 v[18]>UINT32_MAX||v[19]>UINT16_MAX||v[20]||v[21]>UINT32_MAX||(v[21]&0x60)||
 v[22]>UINT32_MAX||(v[22]&0x49)!=0x49||
 !canonical(v[1])||v[1]==UINT64_MAX||!canonical(v[1]+1))return FAT_REJECTED;
 struct fmm_memory m={.memory=memory,.cr0=v[13],.cr3=v[14],.cr4=v[15],
  .efer=v[16],.cs_base=v[17],.cs_ar=(uint32_t)v[18],.cs_selector=(uint16_t)v[19]};
 uint8_t bytes[15];t.available=(unsigned)fmm_fetch(&m,t.rip,bytes);
 if(t.available)t.opcode=bytes[0];
 if(trace)*trace=t;
 if(!t.available||t.opcode!=0xf4)return FAT_REJECTED;
 s->busy=true;
 if(((t.flags&0x10000)&&!store(s,c,w,0x6820,t.flags&~UINT64_C(0x10000)))||
 (t.blocking&&!store(s,c,w,0x4824,0))||!store(s,c,w,0x681e,t.rip+1)||
 !store(s,c,w,0x4826,1))return fatal(s);
 s->resume_rip=t.rip+1;s->halt_flags=t.flags&~UINT64_C(0x10000);
 s->halted=true;s->completed++;s->busy=false;return FAT_COMMITTED;
}
static bool receipt_reason(uint64_t reason){
 switch(reason){case 10:case 14:case 16:case 28:case 29:case 30:case 31:
 case 32:case 40:case 12:case 48:case 52:return true;default:return false;}
}
enum fat_result fhl_after(struct fhl_state*s,bool exclusive,bool proven,bool injected,
 bool receipt,void*c,bool(*r)(void*,uint32_t,uint64_t*),
 bool(*w)(void*,uint32_t,uint64_t),struct fhl_trace*trace){
 if(!exclusive)return FAT_REJECTED;
 if(s&&s->busy)return fatal(s);
 if(!s||s->failed||!s->halted)return FAT_REJECTED;
 if(!proven||!r||!w||injected!=receipt)return fatal(s);
 const uint32_t fields[]={0x4402,0x681e,0x6820,0x4824,0x4826,0x6822,0x681a,0x4016,0x4408,0x4404};
 uint64_t v[sizeof fields/sizeof*fields];
 for(unsigned i=0;i<sizeof fields/sizeof*fields;i++)if(!r(c,fields[i],&v[i]))return fatal(s);
 if(trace)*trace=(struct fhl_trace){.reason=v[0],.rip=v[1],.flags=v[2],.blocking=v[3],.activity=v[4]};
 if(!flags_ok(v[2])||v[3]>2||v[4]>1||v[5]||v[6]!=0x400||v[7]||
 v[8]>UINT32_MAX||(v[8]&0x80000000)||v[9]>UINT32_MAX||(v[9]&0x80000000))return fatal(s);
 if(injected){
  if(!receipt_reason(v[0])||v[4]||s->wakeups==UINT64_MAX)return fatal(s);
  s->halted=false;s->wakeups++;return FAT_COMMITTED;
 }
 if(v[0]!=52||v[1]!=s->resume_rip||v[2]!=s->halt_flags||v[3]||s->idle_exits==UINT64_MAX)return fatal(s);
 s->busy=true;
 if(!v[4]&&!store(s,c,w,0x4826,1))return fatal(s);
 s->idle_exits++;s->busy=false;return FAT_COMMITTED;
}
