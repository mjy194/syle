#include "../vcpu.h"
#include "../ept.h"
#include <assert.h>
#include <stdio.h>
static bool inject(struct vcpu *v) {
    uint32_t out=0;
    bool ok=vcpu_inject_ud(v,0,0x80000306,0,0,0,0,0x6000,0x6000,&out);
    assert(out==(ok?0x80000306u:0));return ok;
}
int main(void) {
    assert(ept_demand_pte_valid(0x8003));
    assert(ept_demand_pte_valid(0x8023));
    assert(!ept_demand_pte_valid(0x8002));
    assert(!ept_demand_pte_valid(0x8043));
    assert(!ept_demand_pte_valid(0x8007));
    assert(!ept_demand_pte_valid(0x9003));
    assert(!ept_demand_pte_valid(0x8001));
    assert(!vcpu_halt_activity_supported(0));
    assert(vcpu_halt_activity_supported(UINT64_C(1)<<6));
    assert(vcpu_halt_activity_supported(UINT64_MAX));
    assert(!vcpu_halt_activity_supported(UINT64_MAX ^ (UINT64_C(1)<<6)));
    for(unsigned bit=0;bit<64;bit++)
        assert(vcpu_halt_activity_supported(UINT64_C(1)<<bit)==(bit==6));
    struct vcpu v={0};
    assert(inject(&v));assert(v.ud_injected && v.state==VCPU_RUNNABLE);
    assert(vcpu_ack_ud(&v));assert(vcpu_halt(&v));assert(v.state==VCPU_HALTED);
    assert(vcpu_complete(&v));assert(v.state==VCPU_COMPLETED);
    assert(!inject(&v));assert(v.state==VCPU_FAULTED);
    for(unsigned i=0;i<9;i++) {
        uint64_t args[9]={0,0x80000306,0,0,0,0,0x6000,0x6000,0};
        /* Reason, info, vectoring, pending, blocking, activity, RIP mismatch. */
        if(i<7) args[i]^= (i==2||i==3)?(1ULL<<31):1;
        v=(struct vcpu){0};
        if(i==7) v.ud_injected=true;
        if(i==8) v.state=VCPU_HALTED;
        uint32_t out=123;
        assert(!vcpu_inject_ud(&v,args[0],args[1],args[2],args[3],args[4],
                              args[5],args[6],args[7],&out));
        assert(v.state==VCPU_FAULTED && out==123);
    }
    const uint64_t bad[]={0x306,0x80000b06,0x80000206,0x8000030e,0x80001306};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) {
        uint32_t out=0;v=(struct vcpu){0};
        assert(!vcpu_inject_ud(&v,0,bad[i],0,0,0,0,1,1,&out));
    }
    v=(struct vcpu){0}; assert(!vcpu_ack_ud(&v));
    v=(struct vcpu){0}; assert(!vcpu_halt(&v));
    v=(struct vcpu){0}; assert(!vcpu_complete(&v));
    v=(struct vcpu){0}; assert(inject(&v)); assert(vcpu_ack_ud(&v)); assert(!vcpu_ack_ud(&v));
    v=(struct vcpu){.state=VCPU_HALTED,.ud_handled=true};
    uint32_t irq=0;
    assert(vcpu_inject_irq(&v,64,0x202,0,1,0,0,&irq));
    assert(irq==0x80000040u && v.state==VCPU_HALTED && v.irq_pending);
    assert(vcpu_ack_irq(&v,0));assert(v.state==VCPU_RUNNABLE && v.irq_handled);
    assert(vcpu_halt(&v));assert(vcpu_complete(&v));
    for(unsigned i=0;i<11;i++) {
        v=(struct vcpu){.state=VCPU_HALTED,.ud_handled=true};
        uint32_t vec=64,out=123;uint64_t flags=0x202,blocking=0,activity=1,pending=0,vectoring=0;
        if(i==0) vec=31;
        if(i==1) vec=256;
        if(i==2) flags=2;
        if(i==3) blocking=1; /* STI */
        if(i==4) blocking=2; /* MOVSS */
        if(i==5) activity=0;
        if(i==6) pending=1ULL<<31;
        if(i==7) vectoring=1ULL<<31;
        if(i==8) v.irq_pending=true;
        if(i==9) v.irq_handled=true;
        if(i==10) v.state=VCPU_RUNNABLE;
        assert(!vcpu_inject_irq(&v,vec,flags,blocking,activity,pending,vectoring,&out));
        assert(out==123 && v.state==VCPU_FAULTED);
    }
    v=(struct vcpu){.state=VCPU_HALTED,.irq_pending=true};
    assert(!vcpu_ack_irq(&v,1));
    v=(struct vcpu){.state=VCPU_HALTED};
    assert(!vcpu_ack_irq(&v,0));
    v=(struct vcpu){.state=VCPU_RUNNABLE,.ud_handled=true};
    uint32_t gi=0,ge=0;
    assert(vcpu_inject_gp(&v,0,0x80000b0d,0x28,0,0,0,0,0x6000,0x6000,&gi,&ge));
    assert(gi==0x80000b0d && ge==0x28 && v.gp_pending);
    assert(vcpu_ack_gp(&v));assert(v.gp_handled && !v.gp_pending);
    assert(!vcpu_ack_gp(&v));
    for(unsigned i=0;i<12;i++) {
        uint64_t a[9]={0,0x80000b0d,0x28,0,0,0,0,0x6000,0x6000};
        v=(struct vcpu){.state=VCPU_RUNNABLE,.ud_handled=true};
        if(i<8) a[i]^=(i==3||i==4)?(1ULL<<31):1;
        if(i==8) v.gp_pending=true;
        if(i==9) v.gp_handled=true;
        if(i==10) v.ud_handled=false;
        if(i==11) v.state=VCPU_HALTED;
        gi=123;ge=456;
        assert(!vcpu_inject_gp(&v,a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],&gi,&ge));
        assert(v.state==VCPU_FAULTED && gi==123 && ge==456);
    }
    v=(struct vcpu){.ud_handled=true};
    assert(!vcpu_inject_gp(&v,0,0x8000030d,0x28,0,0,0,0,1,1,&gi,&ge));
    v=(struct vcpu){.ud_handled=true};
    assert(!vcpu_inject_gp(&v,0,0x80000b0d,0x100000028ULL,0,0,0,0,1,1,&gi,&ge));
    v=(struct vcpu){.ud_handled=true};
    gi=123;ge=456;
    assert(vcpu_inject_pf(&v,0,0x80000b0e,0,0xc000,0,0,0,0,1,1,&gi,&ge));
    assert(gi==0x80000b0e && ge==0 && v.pf_pending);
    assert(vcpu_ack_pf(&v));assert(v.pf_handled);assert(!vcpu_ack_pf(&v));
    for(unsigned i=0;i<10;i++) {
        uint64_t a[10]={0,0x80000b0e,0,0xc000,0,0,0,0,1,1};
        a[i]^=(i==4||i==5)?(1ULL<<31):1;
        v=(struct vcpu){.ud_handled=true};gi=123;ge=456;
        assert(!vcpu_inject_pf(&v,a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],&gi,&ge));
        assert(v.state==VCPU_FAULTED && gi==123 && ge==456);
    }
    v=(struct vcpu){.ud_handled=true,.pf_pending=true};
    assert(!vcpu_inject_pf(&v,0,0x80000b0e,0,0xc000,0,0,0,0,1,1,&gi,&ge));
    puts("PASS vCPU lifecycle and strict #UD injection policy");
}
