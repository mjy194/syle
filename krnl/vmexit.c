#include "vmexit.h"
void vmexit_cpuid(struct vmx_guest_registers *r) {
    uint32_t leaf=(uint32_t)r->rax,subleaf=(uint32_t)r->rcx;
    uint32_t a=0,b=0,c=0,d=0;
    if(subleaf==0) switch(leaf) {
    case 0: a=1;b=0x656c7953;d=0x656c7953;c=0x656c7953;break; /* SyleSyleSyle */
    case 1: c=1u<<31;d=1u<<6;break; /* hypervisor present, PAE only */
    case 0x80000000u: a=0x80000001u;break;
    case 0x80000001u: d=1u<<29;break; /* long mode; no SYSCALL/NX claim */
    case 0x40000000u: a=0x40000000u;b=0x656c7953;c=0x656c7953;d=0x656c7953;break;
    default: break;
    }
    /* Architectural writes of EAX/EBX/ECX/EDX zero the upper halves. */
    r->rax=a;r->rbx=b;r->rcx=c;r->rdx=d;
}
enum vmexit_action vmexit_dispatch(uint64_t reason,uint64_t rip,uint64_t length,
    uint64_t begin,uint64_t end,struct vmx_guest_registers *r,uint64_t *next) {
    if(begin>=end || rip<begin || rip>=end || length>end-rip ||
       rip>UINT64_MAX-length) return VMEXIT_REJECT;
    if(reason==10 && length==2) {
        vmexit_cpuid(r);*next=rip+2;return VMEXIT_RESUME;
    }
    if(reason==12 && length==1) {
        *next=rip+1;return VMEXIT_HALT;
    }
    return VMEXIT_REJECT;
}
