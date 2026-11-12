#include "nested_host_state.h"
#include <stddef.h>
static uint64_t msr(uint32_t index) {
    uint32_t lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(index));
    return ((uint64_t)hi<<32)|lo;
}
static bool interrupts_disabled(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
    return !(flags&(UINT64_C(1)<<9));
}
bool nested_host_state_capture(struct nested_host_state *out) {
    if(!out||!interrupts_disabled())return false;
    struct nested_host_state s={0};
    __asm__ volatile("mov %%cr0,%0":"=r"(s.cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(s.cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(s.cr4));
    __asm__ volatile("mov %%dr7,%0":"=r"(s.dr7));
    s.fs_base=msr(0xc0000100);s.gs_base=msr(0xc0000101);
    s.pat=msr(0x277);s.efer=msr(0xc0000080);
    s.sysenter_cs=msr(0x174);s.sysenter_esp=msr(0x175);s.sysenter_eip=msr(0x176);
    s.debugctl=msr(0x1d9);
    __asm__ volatile("sgdt %0":"=m"(s.gdtr));
    __asm__ volatile("sidt %0":"=m"(s.idtr));
    __asm__ volatile("mov %%cs,%0":"=r"(s.cs));
    __asm__ volatile("mov %%ss,%0":"=r"(s.ss));
    __asm__ volatile("mov %%ds,%0":"=r"(s.ds));
    __asm__ volatile("mov %%es,%0":"=r"(s.es));
    __asm__ volatile("mov %%fs,%0":"=r"(s.fs));
    __asm__ volatile("mov %%gs,%0":"=r"(s.gs));
    __asm__ volatile("str %0":"=r"(s.tr));
    __asm__ volatile("sldt %0":"=r"(s.ldtr));
    if(!interrupts_disabled())return false;
    *out=s;
    return true;
}
bool nested_host_state_matches(const struct nested_host_state *e) {
    if(!e)return false;
    struct nested_host_state s;
    if(!nested_host_state_capture(&s))return false;
    /* Field-wise comparisons deliberately exclude structure padding. */
#define SAME(f) (s.f==e->f)
    return SAME(cr0)&&SAME(cr3)&&SAME(cr4)&&SAME(fs_base)&&SAME(gs_base)&&
        SAME(pat)&&SAME(efer)&&SAME(sysenter_cs)&&SAME(sysenter_esp)&&
        SAME(sysenter_eip)&&SAME(debugctl)&&SAME(dr7)&&
        SAME(gdtr.base)&&SAME(gdtr.limit)&&SAME(idtr.base)&&SAME(idtr.limit)&&
        SAME(cs)&&SAME(ss)&&SAME(ds)&&SAME(es)&&SAME(fs)&&SAME(gs)&&
        SAME(tr)&&SAME(ldtr);
#undef SAME
}
