#include "linux_vcpu_cr.h"
#include "linux_vcpu_policy.h"
#define RAM (UINT64_C(128)*1024*1024)
#define VMXE (UINT64_C(1)<<13)
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool fixed(uint64_t x,uint64_t a,uint64_t b){return !(a&~b)&&(x&a)==a&&!(x&~b);}
static bool cr3_valid(uint64_t x){return !(x&UINT64_C(0xfe7))&&(x&~UINT64_C(0xfff))<=RAM-4096;}
static bool valid(const struct lvc_state *s,const struct lvc_limits *l){return l->ram_size==RAM&&canonical(s->rip)&&canonical(s->rsp)&&lvp_guest_cr0(s->cr0_shadow)&&lvp_guest_cr4(s->cr4_shadow)&&s->cr0==s->cr0_shadow&&s->cr4==(s->cr4_shadow|VMXE)&&fixed(s->cr0,l->cr0_fixed0,l->cr0_fixed1)&&fixed(s->cr4,l->cr4_fixed0,l->cr4_fixed1)&&cr3_valid(s->cr3);}
static unsigned index_of(unsigned r){static const unsigned map[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};return map[r];}
bool lvc_emulate(const struct lvc_state *s,const struct lvc_limits *l,uint64_t q,unsigned len,struct lvc_change *out){
 if(!s||!l||!out||!valid(s,l)||len<3||len>15||s->rip>UINT64_MAX-len||!canonical(s->rip+len)||(q&~UINT64_C(0xf3f)))return false;
 unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;
 if((cr!=0&&cr!=3&&cr!=4)||access>1)return false;
 struct lvc_change c={.next=*s,.changed=0};
 if(access==0){uint64_t v=reg==4?s->rsp:s->gpr[index_of(reg)];
 if(cr==0){if(!lvp_guest_cr0(v)||!fixed(v,l->cr0_fixed0,l->cr0_fixed1))return false;c.next.cr0=c.next.cr0_shadow=v;c.changed=LVC_CR0;}
 else if(cr==3){if(!cr3_valid(v))return false;c.next.cr3=v;c.changed=LVC_CR3;}
 else{if(!lvp_guest_cr4(v)||!fixed(v|VMXE,l->cr4_fixed0,l->cr4_fixed1))return false;c.next.cr4=v|VMXE;c.next.cr4_shadow=v;c.changed=LVC_CR4;}
 }else{uint64_t v=cr==0?s->cr0_shadow:(cr==3?s->cr3:s->cr4_shadow);if(reg==4){c.next.rsp=v;c.changed=LVC_RSP;}else{c.next.gpr[index_of(reg)]=v;c.changed=LVC_GPR;}}
 c.next.rip=s->rip+len;*out=c;return true;
}
