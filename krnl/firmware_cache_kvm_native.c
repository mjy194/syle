/* NONDEFAULT: must only be linked into the separately admitted KVM fixture.
 * This instruction executes in Syle's outer guest, never in the user's OS.
 * Runtime identification is an accident-prevention gate, not a replacement
 * for verifying outer VMCS01 interception and the fixed QEMU device graph.
 * No default target links this file. No OS settings are modified here. */
#include <stdbool.h>
#include <stdint.h>
#ifndef SYLE_CACHE_KVM_FIXTURE
#error "Only compile after scoped KVM fixture admission"
#endif
static void cpuid(uint32_t leaf,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d){
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0):"memory");
}
bool fwb_kvm_environment(void){
 uint32_t a,b,c,d;uint16_t cs;uint64_t flags;
 __asm__ volatile("mov %%cs,%0":"=r"(cs));__asm__ volatile("pushfq; pop %0":"=r"(flags));
 if((cs&3)||(flags&512))return false;
 cpuid(1,&a,&b,&c,&d);if(!(c&(1u<<31)))return false;
 /* QEMU/KVM with Hv leaves uses the secondary KVM hypervisor range. */
 cpuid(0x40000100,&a,&b,&c,&d);
 return a>=0x40000101&&a<0x40000200&&b==0x4b4d564b&&c==0x564b4d56&&d==0x4d;
}
bool fwb_kvm_maintain(void){
 if(!fwb_kvm_environment())return false;
 /* x86-64 CPL0 WBINVD has no memory operand and no optional CPUID feature.
  * The admitted outer KVM owns its physical effect, which may include actual
  * cache maintenance. Do not describe this as physical-domain isolation. */
 __asm__ volatile("mfence; wbinvd; mfence":::"memory");
 return true;
}
