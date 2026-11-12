#include "../linux_vcpu_native.h"
#include <assert.h>
#include <stdio.h>
extern char lvn_rd_fault[],lvn_wr_fault[],lvn_fxsave_fault[],lvn_fxload_fault[],lvn_false[];
extern char lvn_rsp_fault[],lvn_rip_fault[],lvn_early_fail[];
extern char lvn_launch_fault[],lvn_resume_fault[],lvn_entry_fail[];
extern int lvn_fx_user_probe(const uint8_t *,uint8_t *);
int main(void){
 char *sites[]={lvn_rd_fault,lvn_wr_fault,lvn_fxsave_fault,lvn_fxload_fault,
               lvn_rsp_fault,lvn_rip_fault,lvn_launch_fault,lvn_resume_fault};
 for(unsigned i=0;i<8;i++)for(unsigned v=0;v<32;v++){
  uint64_t ip=(uintptr_t)sites[i],old=ip;lvn_armed_rip=ip;
  bool good=lvn_fault_fixup(v,&ip);
  if(v==6||v==13){assert(good&&!lvn_armed_rip);
   assert(ip==(uintptr_t)(i<4?lvn_false:(i<6?lvn_early_fail:lvn_entry_fail)));}
  else assert(!good&&ip==old&&lvn_armed_rip==old);
 }
 uint64_t ip=(uintptr_t)lvn_rd_fault,old=ip;lvn_armed_rip=ip+1;
 assert(!lvn_fault_fixup(13,&ip)&&ip==old);lvn_armed_rip=0;
 assert(!lvn_fault_fixup(13,&ip)&&!lvn_fault_fixup(13,NULL));
 ip=1;lvn_armed_rip=1;assert(!lvn_fault_fixup(13,&ip)&&ip==1);lvn_armed_rip=0;
 struct lvn_context c={0};struct lvs_ops ops={0};uint64_t out=123;
 assert(!lvn_bind(&c,&ops)&&!c.enabled&&!ops.enter);
 c.enabled=true;assert(!lvn_enter(&c));
 assert(!lvn_read_msr(NULL,0x480,&out)&&out==123);
 assert(!lvn_write_msr(&c,0xc0000101,0));
 _Alignas(16) uint8_t reset[512]={0},seen[512]={0};
 reset[0]=0x7f;reset[1]=3;reset[24]=0x80;reset[25]=0x1f;
 assert(lvn_fx_user_probe(reset,seen));
 assert(seen[0]==0x7f&&seen[1]==3&&seen[4]==0&&seen[24]==0x80&&seen[25]==0x1f);
 assert(!lvn_armed_rip);
 puts("linux_vcpu_native: exact fixup matrix, CPL3 deny, user FX roundtrip PASS (no VM)");
}
