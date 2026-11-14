#include "../firmware_cpu_mode_de.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define RAM UINT64_C(0x8000000)
static struct fcmd_gate gate={.host_leaf1_edx=4,.primary_controls=1u<<23,.dr7=0x400,.entry_controls=4,.exit_controls=4,.cr4_fixed1=UINT64_MAX};
static void reject(struct fcm_state*s,enum fcm_register reg,uint64_t v,struct fcmd_gate*g){struct fcm_state out,old;memset(&out,0xa5,sizeof out);old=out;assert(!fcmd_write(s,RAM,reg,v,g,&out)&&!memcmp(&out,&old,sizeof out));}
int main(void){
 struct fcm_state s={.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x660,.efer=0x500,.ia32e=true},n;
 assert(!fcm_write(&s,RAM,FCM_CR4,0x668,&n));
 assert(fcmd_write(&s,RAM,FCM_CR4,0x668,&gate,&n)&&n.cr4==0x668&&n.cr0==s.cr0&&n.cr3==s.cr3&&n.efer==s.efer&&n.ia32e);
 assert(!fcm_write(&n,RAM,FCM_CR3,0x1000,&s));
 assert(fcmd_write(&n,RAM,FCM_CR3,0x1000,&gate,&n)&&n.cr4==0x668&&n.cr3==0x1000);
 assert(fcmd_write(&n,RAM,FCM_CR0,0x80010023,&gate,&n)&&n.cr0==0x80010033&&n.cr4==0x668);
 assert(fcmd_write(&n,RAM,FCM_EFER,0x500,&gate,&n)&&n.cr4==0x668);
 assert(fcmd_write(&n,RAM,FCM_CR4,0x660,&gate,&n)&&n.cr4==0x660);
 reject(&n,FCM_CR4,0x2688,&gate);reject(&n,FCM_CR0,0x33,&gate);
 reject(&n,FCM_CR4,8,&gate);reject(&n,FCM_CR3,RAM,&gate);reject(&n,FCM_EFER,0x400,&gate);
 for(unsigned k=0;k<10;k++){struct fcmd_gate bad=gate;
  switch(k){case 0:bad.host_leaf1_edx=0;break;case 1:bad.primary_controls=0;break;
  case 2:bad.debugctl=1;break;case 3:bad.dr7=0x401;break;case 4:bad.primary_controls|=UINT64_C(1)<<32;break;
  case 5:bad.pending_debug=1;break;case 6:bad.entry_controls=0;break;
  case 7:bad.exit_controls=0;break;case 8:bad.cr4_fixed1=0;break;
  case 9:bad.cr4_fixed0=UINT64_C(1)<<63;break;}
  reject(&n,FCM_CR4,0x668,&bad);reject(&n,FCM_CR3,0x2000,&bad);
 }
 /* Default equivalence for all DE0 register values in a bounded sweep. */
 for(unsigned reg=0;reg<4;reg++)for(uint64_t v=0;v<4096;v++){
  if(reg==FCM_CR4&&(v&8))continue;
  struct fcm_state a,b;bool x=fcm_write(&n,RAM,(enum fcm_register)reg,v,&a);
  bool y=fcmd_write(&n,RAM,(enum fcm_register)reg,v,&gate,&b);assert(x==y);
  if(x)assert(a.cr0==b.cr0&&a.cr3==b.cr3&&a.cr4==b.cr4&&a.efer==b.efer&&a.ia32e==b.ia32e);
 }
 /* DE persists through the baseline supported reset-to-long-mode sequence. */
 s=(struct fcm_state){.cr0=0x10,.cr4=8};
 assert(fcmd_write(&s,RAM,FCM_CR0,0x11,&gate,&s));
 assert(fcmd_write(&s,RAM,FCM_CR4,0x28,&gate,&s));
 assert(fcmd_write(&s,RAM,FCM_EFER,0x100,&gate,&s));
 assert(fcmd_write(&s,RAM,FCM_CR0,0x80000011,&gate,&s)&&s.ia32e&&s.cr4==0x28&&s.efer==0x500);
 assert(!fcmd_write(0,RAM,FCM_CR4,8,&gate,&n));assert(!fcmd_write(&s,RAM,FCM_CR4,8,0,&n));assert(!fcmd_write(&s,RAM,FCM_CR4,8,&gate,0));
 reject(&s,(enum fcm_register)99,0,&gate);
 puts("firmware_cpu_mode_de: PASS (configuration only; all MOVDR remain fail-stop)");
}
