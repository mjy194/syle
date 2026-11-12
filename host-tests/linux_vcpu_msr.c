#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "linux_vcpu_msr.h"
static struct lvm_gs_state s;static struct lvm_gs_change out;
static void reset(void){s=(struct lvm_gs_state){.rax=0xdeadbeef12345678ULL,.rdx=0xcafebabeffff8000ULL,.rcx=0x12345678c0000101ULL,.rip=0x200000,.gs_base=0xffff800012345678ULL};}
static void no(uint64_t reason,unsigned len){memset(&out,0x5a,sizeof out);struct lvm_gs_change prior=out;assert(!lvm_gs_emulate(&s,reason,len,&out));assert(!memcmp(&out,&prior,sizeof out));}
int main(void){reset();assert(lvm_gs_emulate(&s,32,2,&out));assert(out.write_vmcs&&out.field==0x6810&&out.value==s.gs_base&&out.next.gs_base==s.gs_base&&out.next.rip==s.rip+2&&out.next.rax==s.rax&&out.next.rdx==s.rdx&&out.next.rcx==s.rcx);
 assert(lvm_gs_emulate(&s,31,2,&out));assert(!out.write_vmcs&&out.field==0&&out.value==0&&out.next.rax==0x12345678&&out.next.rdx==0xffff8000&&out.next.gs_base==s.gs_base&&out.next.rcx==s.rcx);
 reset();s.rax=0;s.rdx=0;assert(lvm_gs_emulate(&s,32,2,&out));assert(out.value==0);
 reset();s.rdx=0x8000;s.rax=0;no(32,2);s.rdx=0xffff7fff;no(32,2);
 reset();s.cpl=3;no(32,2);s.cpl=4;no(31,2);
 reset();s.rcx=0xc0000102;no(32,2);reset();s.rcx=0x174;no(31,2);
 reset();no(30,2);no(33,2);no((1ULL<<31)|32,2);no(32,0);no(32,1);no(32,3);no(32,16);
 reset();s.rip=UINT64_MAX;no(32,2);s.rip=0x7ffffffffffe;no(31,2);s.rip=1ULL<<48;no(31,2);
 reset();s.gs_base=1ULL<<48;no(31,2);no(32,2);
 reset();assert(!lvm_gs_emulate(NULL,32,2,&out));assert(!lvm_gs_emulate(&s,32,2,NULL));
 reset();s.rip=0xffff800000000000ULL;assert(lvm_gs_emulate(&s,31,2,&out));
 puts("linux_vcpu_msr: PASS; pure guest GS_BASE staged emulation only");}
