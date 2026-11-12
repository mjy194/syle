#include "linux_vcpu_msr.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lvm_gs_emulate(const struct lvm_gs_state *s,uint64_t reason,unsigned len,
 struct lvm_gs_change *out){
 if(!s||!out||s->cpl!=0||(reason!=31&&reason!=32)||len!=2||
    (uint32_t)s->rcx!=UINT32_C(0xc0000101)||!canonical(s->rip)||
    s->rip>UINT64_MAX-2||!canonical(s->rip+2)||!canonical(s->gs_base))return false;
 struct lvm_gs_change c={.next=*s};
 if(reason==32){
  uint64_t v=((uint64_t)(uint32_t)s->rdx<<32)|(uint32_t)s->rax;
  if(!canonical(v))return false;
  c.next.gs_base=v;c.write_vmcs=true;c.field=0x6810;c.value=v;
 }else{
  c.next.rax=(uint32_t)s->gs_base;c.next.rdx=(uint32_t)(s->gs_base>>32);
 }
 c.next.rip=s->rip+2;*out=c;return true;
}
