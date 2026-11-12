#include <assert.h>
#include <stdio.h>
#include "../vmx_memory.h"
#define PAGE UINT64_C(4096)
static struct vmx_memory_snapshot baseline(void) {
    struct vmx_memory_snapshot s={.physical_bits=48,.cpu_mtrr=1,.cpu_pat=1,
        .def_type=0x806,.pat=6};
    return s;
}
static void range(struct vmx_memory_snapshot *s,unsigned n,uint64_t base,uint64_t size,unsigned type) {
    s->variable[n].base=base|type;
    s->variable[n].mask=(((UINT64_C(1)<<s->physical_bits)-1)&~(size-1))|0x800;
    s->cap=(s->cap&~UINT64_C(255))|(n+1);
}
int main(void) {
    struct vmx_memory_snapshot s=baseline();
    assert(vmx_memory_snapshot_region_wb(&s,0x4000000));
    assert(vmx_memory_snapshot_region_wb(&s,UINT64_C(0x100000000)));
    assert(vmx_memory_snapshot_region_wb(&s,(UINT64_C(1)<<48)-PAGE));
    assert(!vmx_memory_snapshot_region_wb(&s,UINT64_C(1)<<48));
    assert(!vmx_memory_snapshot_region_wb(&s,1));
    assert(!vmx_memory_snapshot_region_wb(0,0));
#define BAD(field,val) do { s=baseline();s.field=(val);assert(!vmx_memory_snapshot_region_wb(&s,0x4000000)); } while(0)
    BAD(cpu_mtrr,0);BAD(cpu_pat,0);BAD(physical_bits,31);BAD(physical_bits,53);
    BAD(cr0,UINT64_C(1)<<29);BAD(cr0,UINT64_C(1)<<30);
    BAD(pat,0);BAD(pat,4);BAD(def_type,6);BAD(def_type,0x802);BAD(def_type,0x100806);
    BAD(def_type,0xc06); /* fixed enabled without support */
    s=baseline();s.def_type=0x800;range(&s,0,0x4000000,0x200000,6);
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000-PAGE));
    assert(vmx_memory_snapshot_region_wb(&s,0x4000000));
    assert(vmx_memory_snapshot_region_wb(&s,0x4200000-PAGE));
    assert(!vmx_memory_snapshot_region_wb(&s,0x4200000));
    range(&s,1,0x4000000,PAGE,6);
    assert(vmx_memory_snapshot_region_wb(&s,0x4000000));
    s.variable[1].base=0x4000004; /* WB / WT is WT */
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    s.variable[1].base=0x4000000; /* UC dominates */
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    s.variable[1].base=0x4000001; /* undefined mix */
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    s.variable[1].mask=0; /* disabled range ignored */
    assert(vmx_memory_snapshot_region_wb(&s,0x4000000));
    s.variable[0].mask|=0x1000; /* noncontiguous */
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    s=baseline();range(&s,0,0x4001000,0x200000,6); /* base misalignment */
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    s=baseline();range(&s,0,0x4000000,0x200000,6);s.variable[0].base|=UINT64_C(1)<<48;
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    s=baseline();range(&s,0,0x4000000,0x200000,6);s.variable[0].mask|=0x100;
    assert(!vmx_memory_snapshot_region_wb(&s,0x4000000));
    /* All 88 fixed-MTRR intervals, and their first/last pages. */
    s=baseline();s.cap=0x100;s.def_type=0xc00;
    for (unsigned reg=0;reg<11;++reg) {
        for (unsigned byte=0;byte<8;++byte) {
            uint64_t base,step;
            if (!reg) { base=0;step=0x10000; }
            else if (reg<3) { base=0x80000+(reg-1)*0x20000;step=0x4000; }
            else { base=0xc0000+(reg-3)*0x8000;step=0x1000; }
            s.fixed[reg]=UINT64_C(6)<<(byte*8);
            assert(vmx_memory_snapshot_region_wb(&s,base+byte*step));
            assert(vmx_memory_snapshot_region_wb(&s,base+(byte+1)*step-PAGE));
            s.fixed[reg]=0;
            assert(!vmx_memory_snapshot_region_wb(&s,base+byte*step));
        }
    }
    /* Fixed ranges override a matching UC variable range. */
    s.fixed[0]=6;range(&s,0,0,0x100000,0);
    assert(vmx_memory_snapshot_region_wb(&s,0));
    assert(!vmx_memory_snapshot_region_wb(&s,0x100000));
    puts("PASS VMX memory-type policy (synthetic snapshots, no hardware MSR test)");
}
