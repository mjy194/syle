#include <assert.h>
#include <string.h>
#include "../vmexit.h"
int main(void) {
    struct vmx_guest_registers r={0},old;
    uint64_t next=0xaaaa;
    r.rax=0xdeadbeef00000000ULL;r.rcx=0x1234567800000000ULL;
    r.rbx=~0ULL;r.rdx=~0ULL;r.rsi=0x111;r.r15=0x222;
    assert(vmexit_dispatch(10,0x6000,2,0x6000,0x7000,&r,&next)==VMEXIT_RESUME);
    assert(next==0x6002&&r.rax==1&&r.rbx==0x656c7953&&r.rcx==0x656c7953&&r.rdx==0x656c7953);
    assert(r.rsi==0x111&&r.r15==0x222);
    r.rax=1;r.rcx=0;vmexit_cpuid(&r);
    assert(r.rax==0&&r.rbx==0&&r.rcx==0x80000000ULL&&r.rdx==0x40);
    assert(!(r.rcx&(1u<<5))); /* VMX not exposed */
    r.rax=0x80000000;r.rcx=0;vmexit_cpuid(&r);assert(r.rax==0x80000001);
    r.rax=0x80000001;r.rcx=0;vmexit_cpuid(&r);assert(r.rdx==(1u<<29)&&r.rax==0&&r.rcx==0);
    r.rax=0x40000000;r.rcx=0;vmexit_cpuid(&r);
    assert(r.rax==0x40000000&&r.rbx==0x656c7953&&r.rcx==0x656c7953&&r.rdx==0x656c7953);
    const uint32_t unknown[]={2,7,0xd,0x40000001,0x80000002,0xffffffff};
    for(unsigned i=0;i<sizeof unknown/sizeof *unknown;i++) {
        r.rax=unknown[i];r.rcx=0;r.rbx=~0ULL;r.rdx=~0ULL;vmexit_cpuid(&r);
        assert(!(r.rax|r.rbx|r.rcx|r.rdx));
    }
    r.rax=0;r.rcx=1;vmexit_cpuid(&r);assert(!(r.rax|r.rbx|r.rcx|r.rdx));
    old=r;
    assert(vmexit_dispatch(12,0x6000,1,0x6000,0x7000,&r,&next)==VMEXIT_HALT);
    assert(next==0x6001&&!memcmp(&old,&r,sizeof r));
    const uint64_t bad[][5]={
        {10|0x80000000u,0x6000,2,0x6000,0x7000},
        {18,0x6000,3,0x6000,0x7000}, {48,0x6000,3,0x6000,0x7000},
        {10,0x6000,1,0x6000,0x7000},{12,0x6000,2,0x6000,0x7000},
        {10,0x5fff,2,0x6000,0x7000},{10,0x6fff,2,0x6000,0x7000},
        {10,UINT64_MAX-1,2,UINT64_MAX-2,UINT64_MAX},
        {10,0x6000,2,0x7000,0x6000}
    };
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) {
        next=0xaaaa;r=old;
        assert(vmexit_dispatch(bad[i][0],bad[i][1],bad[i][2],bad[i][3],bad[i][4],&r,&next)==VMEXIT_REJECT);
        assert(next==0xaaaa&&!memcmp(&r,&old,sizeof r));
    }
}
