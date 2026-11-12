#include "linux_efer_commit.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lefer_apply(uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned len,const struct lefer_env*e,void*ctx,bool(*write)(void*,uint32_t,uint64_t),bool(*invept)(void*,uint64_t)){
 if(!g||!e||!write||!invept||cpl||(reason!=31&&reason!=32)||len!=2||(uint32_t)g[2]!=0xc0000080||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 const uint64_t allowed=0xd01;
 if((e->efer&~allowed)||(e->efer&0x500)!=0x500||!(e->cr0&(1ULL<<31))||!(e->cr4&(1ULL<<5))||!(e->entry&(1u<<9)))return false;
 if(((e->efer&1)&&!(e->ext_edx&(1u<<11)))||((e->efer&0x800)&&!(e->ext_edx&(1u<<20))))return false;
 uint64_t next=e->efer,rax=g[0],rdx=g[3];
 if(reason==32){
  uint64_t input=((uint64_t)(uint32_t)g[3]<<32)|(uint32_t)g[0];
  if((input&~allowed)||(input&0x500)!=(e->efer&0x500)||((input&1)&&!(e->ext_edx&(1u<<11)))||((input&0x800)&&!(e->ext_edx&(1u<<20))))return false;
  next=(input&0x801)|(e->efer&0x500);
  if(!write(ctx,0x2806,next))return false;
  /* No VPID; discard EPT-derived combined translations on NXE changes. */
  if(((next^e->efer)&0x800)&&!invept(ctx,e->eptp))return false;
 }else{rax=(uint32_t)next;rdx=(uint32_t)(next>>32);}
 if(!write(ctx,0x681e,rip+2))return false;
 g[0]=rax;g[3]=rdx;return true;
}
