#define main baseline_tests
#include "linux_vcpu_prepare.c"
#undef main
bool lvp_prepare_diagnostic_pf(const struct linux_guest_memory*,const struct lvp_caps*,struct lvp_plan*);
int main(void){
 baseline_tests();struct lvp_plan diagnostic;
 assert(lvp_prepare_diagnostic_pf(&m,&c,&diagnostic));
 assert(diagnostic.count==p.count&&!memcmp(diagnostic.gpr,p.gpr,sizeof p.gpr));
 unsigned bitmap=0,mask=0,match=0;
 for(unsigned i=0;i<p.count;i++){
  assert(p.fields[i].encoding==diagnostic.fields[i].encoding);
  if(p.fields[i].encoding==0x4004){
   bitmap=(unsigned)p.fields[i].value;
   assert(diagnostic.fields[i].value==UINT32_MAX);
   assert((p.fields[i].value^diagnostic.fields[i].value)==(1u<<14));
  }else assert(p.fields[i].value==diagnostic.fields[i].value);
  if(p.fields[i].encoding==0x4006)mask=(unsigned)p.fields[i].value;
  if(p.fields[i].encoding==0x4008)match=(unsigned)p.fields[i].value;
 }
 assert(bitmap==(UINT32_MAX&~(1u<<14))&&!mask&&!match);
 /* Intel SDM 26.2: inequality reverses exception bitmap bit14. */
 for(unsigned ec=0;ec<=65535;ec++){
  bool equality=(ec&mask)==match;
  bool exits=equality?!!(bitmap&(1u<<14)):!(bitmap&(1u<<14));assert(!exits);
 }
 assert((UINT32_MAX&mask)==match);
 m.prepared=false;memset(&diagnostic,0xa5,sizeof diagnostic);before=diagnostic;
 assert(!lvp_prepare_diagnostic_pf(&m,&c,&diagnostic)&&!memcmp(&diagnostic,&before,sizeof diagnostic));
 puts("native PF plan: only bit14 changes, mask/match semantic, EPT/other fields identical PASS (host only)");
}
