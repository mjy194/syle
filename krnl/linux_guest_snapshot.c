#include "linux_guest_snapshot.h"
#include "linux_guest_session.h"
#include "linux_pt_permissions.h"
#include "linux_high_pf_data.h"
static bool read64(void*p,uint64_t gpa,uint64_t*out){
 struct lgs_session*s=p;
 if(!s||!out||!s->memory.prepared||!s->memory.owner.map||gpa>LGM_RAM_SIZE-8)return false;
 uint64_t v=0;
 for(unsigned i=0;i<8;i++){
  uint64_t a=gpa+i,pa=s->memory.pages[a>>12];if(!pa||(pa&4095))return false;
  const uint8_t*b=s->memory.owner.map(s->memory.owner.context,pa);if(!b)return false;
  v|=(uint64_t)b[a&4095]<<(8*i);
 }
 *out=v;return true;
}
static bool virt(struct lgs_session*s,uint64_t cr3,uint64_t va,uint64_t*out){
 struct lpt_trace t;lpt_walk(cr3,va,LGM_RAM_SIZE,s,read64,&t);
 return t.status==1&&read64(s,t.gpa,out);
}
bool lgds_high(struct lgs_session*s,uint64_t reason,uint64_t rip,uint64_t fault,
 uint64_t event,uint64_t vector,struct lhp_snapshot*out){
 if(!s||!out||!s->platform.read_field)return false;
 struct lhp_snapshot x={0};
 x.v[H_REASON]=reason;x.v[H_RIP]=rip;x.v[H_ADDR]=fault;x.v[H_EVENT]=event;x.v[H_VECTOR]=vector;
 x.v[H_RSI]=s->native.gpr[4];x.v[H_RDI]=s->native.gpr[5];x.v[H_RCX]=s->native.gpr[2];
 const uint32_t fields[]={0x4404,0x4406,0x6802,0x4826,0x4824,0x6800,0x6804,0x2806,0x681c,0x0802,0x4816,0x0804,0x4818,0x6816,0x4810,0x6818,0x4812};
 const unsigned slots[]={H_INFO,H_ERROR,H_CR3,H_ACTIVITY,H_BLOCKING,H_CR0,H_CR4,H_EFER,H_RSP,H_CS,H_CSAR,H_SS,H_SSAR,H_GDTR,H_GDTLIMIT,H_IDTR,H_IDTLIMIT};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
  if(!s->platform.read_field(s->platform.ctx,fields[i],&x.v[slots[i]]))return false;
 uint64_t cr3=x.v[H_CR3],cr0=x.v[H_CR0],cr4=x.v[H_CR4],efer=x.v[H_EFER];
 if(!virt(s,cr3,0xffffffff81611010ULL,&x.v[H_CSDESC])||!virt(s,cr3,0xffffffff8174a0e0ULL,&x.v[H_GATELO])||
    !virt(s,cr3,0xffffffff8174a0e8ULL,&x.v[H_GATEHI])||!read64(s,0x16f5888,&x.v[H_ROOTENTRY]))return false;
 uint64_t a,b;if(!virt(s,cr3,0xffffffff816cf508ULL,&a)||!virt(s,cr3,0xffffffff816cf510ULL,&b))return false;
 x.v[H_BYTES]=(a>>56)|((b&255)<<8);
 const uint64_t starts[]={0xffffffff8174a0e0ULL,0xffffffff81611010ULL,0xffffffff816cf50fULL,0xffffffff8174d760ULL,0xffffffff81602f38ULL,0xffffffff816cf3deULL,0xffffffff816cf480ULL,0xffffffff816cf550ULL,0xffffffff816cf6c0ULL,0xffffffff81200c7eULL,0xffffffff81736000ULL,0xffffffff816f5000ULL,0xffffffff816f6000ULL,0xffffffff81706000ULL,0xffffffff81716000ULL,0xffffffff81726000ULL,0xffffffff81634010ULL};
 const uint64_t sizes[]={16,8,2,4096,4096,7,58,368,65,29,8,4096,65536,65536,65536,65536,16};
 const unsigned access[]={0,0,2,1,1,2,2,2,2,2,1,1,1,1,1,1,0};
 x.permissions=true;
 for(unsigned i=0;i<sizeof starts/sizeof starts[0];i++)
  if(!lpp_range(cr3,starts[i],sizes[i],access[i],cr0,cr4,efer,LGM_RAM_SIZE,s,read64))x.permissions=false;
 uint64_t counter,base,flags;
 if(!virt(s,cr3,0xffffffff81736000ULL,&counter)||!virt(s,cr3,0xffffffff81634010ULL,&base)||
    !virt(s,cr3,0xffffffff81634018ULL,&flags))return false;
 if(!lhpd_metadata(counter,base,flags,efer))x.permissions=false;
 *out=x;return true;
}
