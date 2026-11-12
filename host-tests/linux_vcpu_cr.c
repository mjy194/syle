#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "linux_vcpu_cr.h"
static struct lvc_state s;static struct lvc_limits l;static struct lvc_change c;
static void reset(void){s=(struct lvc_state){.rsp=0x8000,.rip=0x1000200,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2020,.cr0_shadow=0x80010033,.cr4_shadow=0x20};l=(struct lvc_limits){.cr0_fixed0=0x80000021,.cr0_fixed1=UINT64_MAX,.cr4_fixed0=0x2000,.cr4_fixed1=UINT64_MAX,.ram_size=128*1024*1024};}
static void no(uint64_t q,unsigned len){memset(&c,0x5a,sizeof c);struct lvc_change old=c;assert(!lvc_emulate(&s,&l,q,len,&c));assert(!memcmp(&c,&old,sizeof c));}
int main(void){static const unsigned map[]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};for(unsigned r=0;r<16;r++){
 reset();s.gpr[map[r]]=0x123018;if(r==4)s.rsp=0x123018;assert(lvc_emulate(&s,&l,3|(r<<8),3,&c));assert(c.next.cr3==0x123018&&c.changed==LVC_CR3&&c.next.rip==s.rip+3);
 reset();assert(lvc_emulate(&s,&l,4|0x10|(r<<8),4,&c));assert((r==4?c.next.rsp:c.next.gpr[map[r]])==0x20);assert(c.changed==(r==4?LVC_RSP:LVC_GPR));}
 reset();s.gpr[0]=0x6b0;assert(lvc_emulate(&s,&l,4,3,&c));assert(c.next.cr4==0x26b0&&c.next.cr4_shadow==0x6b0);
 reset();s.gpr[0]=0x8001003b;assert(lvc_emulate(&s,&l,0,3,&c));assert(c.next.cr0==s.gpr[0]);
 reset();s.gpr[0]=0x2020;no(4,3);s.gpr[0]=0x40020;no(4,3);
 reset();s.gpr[0]=0x80010037;no(0,3);s.gpr[0]=0x10033;no(0,3);
 reset();s.gpr[0]=128*1024*1024;no(3,3);s.gpr[0]=(128*1024*1024)-4096+24;assert(lvc_emulate(&s,&l,3,3,&c));s.gpr[0]=0x1001;no(3,3);s.gpr[0]=1ULL<<63;no(3,3);
 reset();no(2,3);no(0x20,3);no(0x30,3);no(0x40,3);no(0x80,3);no(1ULL<<16,3);no(1ULL<<63,3);no(0,2);no(0,16);
 s.rip=UINT64_MAX-1;no(0x10,3);s.rip=0x7ffffffffffe;no(0x10,3);s.rip=1ULL<<48;no(0x10,3);
 reset();s.cr4_shadow=0x2020;no(0x10,3);reset();s.cr0_shadow++;no(0x10,3);
 reset();l.cr4_fixed1&=~0x2000ULL;no(0x10,3);reset();l.ram_size--;no(0x10,3);
 reset();assert(!lvc_emulate(NULL,&l,0,3,&c));assert(!lvc_emulate(&s,NULL,0,3,&c));assert(!lvc_emulate(&s,&l,0,3,NULL));
 puts("linux_vcpu_cr: PASS; pure staged emulation, no VM-entry");}
