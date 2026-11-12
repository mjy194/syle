#include "../smp_vmx.h"
#include <assert.h>
#include <stdio.h>
static struct vmx_caps good(void)
{
    struct vmx_caps c={0};
    c.feature_control=5;c.basic=7|((uint64_t)4096<<32)|((uint64_t)6<<50);
    c.cr0=0x80000011;c.cr4=0x20;
    c.cr0_fixed0=0x80000031;c.cr0_fixed1=UINT64_MAX;
    c.cr4_fixed0=0x20;c.cr4_fixed1=UINT64_MAX;
    return c;
}
static void fail(struct vmx_caps c,uint64_t hpa,unsigned bits,enum smp_vmx_status expected)
{
    uint64_t value=0xabcdef;
    assert(smp_vmx_policy(&c,hpa,bits,&value)==expected);
    assert(value==0xabcdef);
}
int main(void)
{
    struct vmx_caps c=good();uint64_t value;
    assert(smp_vmx_policy(&c,0x1000,48,&value)==SMP_VMX_OK);
    assert(value==(c.cr0|0x20));
    c.cr0_fixed0&=~(uint64_t)0x20;
    assert(smp_vmx_policy(&c,0x1000,48,&value)==SMP_VMX_OK&&value==c.cr0);
    c=good();c.feature_control=4;fail(c,0x1000,48,SMP_VMX_BLOCKED_FEATURE_CONTROL);
    c.feature_control=1;fail(c,0x1000,48,SMP_VMX_BLOCKED_FEATURE_CONTROL);
    c=good();c.basic|=1ull<<31;fail(c,0x1000,48,SMP_VMX_BLOCKED_BASIC);
    c=good();c.basic&=~(0x1fffull<<32);fail(c,0x1000,48,SMP_VMX_BLOCKED_BASIC);
    c.basic|=4097ull<<32;fail(c,0x1000,48,SMP_VMX_BLOCKED_BASIC);
    c=good();c.basic&=~(15ull<<50);fail(c,0x1000,48,SMP_VMX_BLOCKED_BASIC);
    c=good();fail(c,0,48,SMP_VMX_BLOCKED_ADDRESS);fail(c,0x1001,48,SMP_VMX_BLOCKED_ADDRESS);
    fail(c,1ull<<48,48,SMP_VMX_BLOCKED_ADDRESS);fail(c,0x1000,31,SMP_VMX_BLOCKED_ADDRESS);
    fail(c,0x1000,53,SMP_VMX_BLOCKED_ADDRESS);
    c.basic|=1ull<<48;fail(c,1ull<<32,48,SMP_VMX_BLOCKED_ADDRESS);
    assert(smp_vmx_policy(&c,0xfffff000,48,&value)==SMP_VMX_OK);
    c=good();c.cr0_fixed0|=4;fail(c,0x1000,48,SMP_VMX_BLOCKED_CR0);
    c=good();c.cr0_fixed1&=~1ull;fail(c,0x1000,48,SMP_VMX_BLOCKED_CR0);
    c=good();c.cr0_fixed1&=~0x20ull;fail(c,0x1000,48,SMP_VMX_BLOCKED_CR0);
    c=good();c.cr4|=1ull<<13;fail(c,0x1000,48,SMP_VMX_BLOCKED_CR4);
    c=good();c.cr4_fixed0|=1;fail(c,0x1000,48,SMP_VMX_BLOCKED_CR4);
    c=good();c.cr4_fixed1&=~(1ull<<13);fail(c,0x1000,48,SMP_VMX_BLOCKED_CR4);
    assert(smp_vmx_policy(0,0x1000,48,&value)==SMP_VMX_BAD_CONTEXT);
    assert(smp_vmx_policy(&c,0x1000,48,0)==SMP_VMX_BAD_CONTEXT);
    puts("smp_vmx policy: PASS");
}
