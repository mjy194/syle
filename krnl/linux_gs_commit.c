#include "linux_gs_commit.h"
bool lgsc_apply(uint64_t gpr[15],uint64_t rip,uint64_t gs,unsigned cpl,
 uint64_t reason,unsigned length,void *ctx,bool (*write)(void*,uint32_t,uint64_t)){
 if(!gpr||!write)return false;
 struct lvm_gs_state s={gpr[0],gpr[2],gpr[3],rip,gs,cpl};
 struct lvm_gs_change c;
 if(!lvm_gs_emulate(&s,reason,length,&c))return false;
 if(c.write_vmcs&&!write(ctx,c.field,c.value))return false;
 if(!write(ctx,0x681e,c.next.rip))return false;
 gpr[0]=c.next.rax;gpr[2]=c.next.rcx;gpr[3]=c.next.rdx;
 return true;
}
