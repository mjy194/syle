#include <assert.h>
#include "../vmx_guest.h"
int main(void) {
    uint32_t out=0;
    assert(vmx_guest_controls(0xffffffff00000000ULL,0x200,0x200,&out)&&out==0x200);
    assert(!vmx_guest_controls(0x0000000000000000ULL,0x200,0x200,&out));
    assert(!vmx_guest_controls(0xffffffff80000000ULL,0,0x7fffffff,&out));
    assert(vmx_guest_controls(0xffffffff00000001ULL,0x200,0x201,&out)&&out==0x201);
    assert(!vmx_guest_controls(0x0000000000000001ULL,0,1,&out));
    uint64_t marker=UINT64_C(0x53594c45564d5831);
    assert(vmx_guest_exit_matches(18,0x4001000,0x4001000,3,marker));
    assert(!vmx_guest_exit_matches(18|(1u<<31),0x4001000,0x4001000,3,marker));
    assert(!vmx_guest_exit_matches(0,0x4001000,0x4001000,3,marker));
    assert(!vmx_guest_exit_matches(18,0x4001001,0x4001000,3,marker));
    assert(!vmx_guest_exit_matches(18,0x4001000,0x4001000,2,marker));
    assert(!vmx_guest_exit_matches(18,0x4001000,0x4001000,3,0));
    struct vmx_guest_registers r = {
        .rax=marker,.rbx=0,.rcx=0x103,.rdx=0x104,.rsi=0x105,.rdi=0x106,
        .rbp=0x107,.r8=0x108,.r9=0x109,.r10=0x10a,.r11=0x10b,
        .r12=0x10c,.r13=0x10d,.r14=0x10e,.r15=0x10f
    };
#define D(p,reason,rip,len,rsp,flags) vmx_guest_dispatch(p,reason,rip,0x1000,len,rsp,0x8000,flags,&r)
    for (unsigned phase=0;phase<3;phase++) {
        r.rax=marker+phase;r.rbx=phase;
        assert(D(phase,18,0x1000,3,0x8000,0x47)==
            (phase==2?VMX_DISPATCH_DONE:VMX_DISPATCH_RESUME));
        assert(D(phase,18|(1u<<31),0x1000,3,0x8000,0x47)==VMX_DISPATCH_REJECT);
        assert(D(phase,0,0x1000,3,0x8000,0x47)==VMX_DISPATCH_REJECT);
        assert(D(phase,18,0x1001,3,0x8000,0x47)==VMX_DISPATCH_REJECT);
        assert(D(phase,18,0x1000,0,0x8000,0x47)==VMX_DISPATCH_REJECT);
        assert(D(phase,18,0x1000,3,0x8008,0x47)==VMX_DISPATCH_REJECT);
        assert(D(phase,18,0x1000,3,0x8000,0x46)==VMX_DISPATCH_REJECT);
        uint64_t *fields[]={&r.rax,&r.rbx,&r.rcx,&r.rdx,&r.rsi,&r.rdi,&r.rbp,
            &r.r8,&r.r9,&r.r10,&r.r11,&r.r12,&r.r13,&r.r14,&r.r15};
        for (unsigned i=0;i<15;i++) {
            *fields[i]^=1;
            assert(D(phase,18,0x1000,3,0x8000,0x47)==VMX_DISPATCH_REJECT);
            *fields[i]^=1;
        }
    }
    assert(D(3,18,0x1000,3,0x8000,0x47)==VMX_DISPATCH_REJECT);
    assert(vmx_guest_dispatch(0,18,UINT64_MAX-1,UINT64_MAX-1,3,0x8000,0x8000,0x47,&r)==VMX_DISPATCH_REJECT);
    return 0;
}
