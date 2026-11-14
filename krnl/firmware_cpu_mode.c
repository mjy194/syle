#include "firmware_cpu_mode.h"
#define PE UINT64_C(1)
#define PG (UINT64_C(1)<<31)
#define PAE (UINT64_C(1)<<5)
#define LME UINT64_C(0x100)
#define LMA UINT64_C(0x400)
static bool valid(const struct fcm_state*s,uint64_t ram){
 if(ram<4096||(ram&4095)||ram>UINT64_C(0xc0000000)||
 (s->cr0&~UINT64_C(0xe005003f))||!(s->cr0&16)||
 (s->cr4&~UINT64_C(0x660))||(s->efer&~UINT64_C(0xd01))||
 (s->cr3&4095)||s->cr3>ram-4096)return false;
 if((s->cr0&PG)&&!(s->cr0&PE))return false;
 if((s->cr0&(1ull<<29))&&!(s->cr0&(1ull<<30)))return false;
 bool lm=(s->cr0&PG)&&(s->efer&LME);
 return (!lm||(s->cr4&PAE))&&s->ia32e==lm&&!!(s->efer&LMA)==lm;
}
bool fcm_write(const struct fcm_state*s,uint64_t ram,enum fcm_register reg,uint64_t v,struct fcm_state*out){
 if(!s||!out||!valid(s,ram))return false;
 struct fcm_state n=*s;
 switch(reg){
 case FCM_CR0:
  /* No CS.L input: this tiny-ROM profile never exits long mode. */
  if(s->ia32e&&!(v&PG))return false;
  /* Intel SDM MOV to CR: ET is hardwired to one; clearing is ignored. */
  n.cr0=v|UINT64_C(0x10);break;
 case FCM_CR3:n.cr3=v;break;
 case FCM_CR4:n.cr4=v;break;
 case FCM_EFER:
  if((v&LMA)!=(s->efer&LMA)||((s->cr0&PG)&&((v^s->efer)&LME)))return false;
  n.efer=v;break;
 default:return false;
 }
 n.ia32e=(n.cr0&PG)&&(n.efer&LME);
 n.efer=(n.efer&~LMA)|(n.ia32e?LMA:0);
 if(!valid(&n,ram))return false;
 *out=n;return true;
}
