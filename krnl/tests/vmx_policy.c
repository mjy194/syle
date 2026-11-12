#include <assert.h>
#include <string.h>
#include "../vmx.h"
int main(void) {
    struct vmx_caps good={.feature_control=5,
        .basic=(6ULL<<50)|(4096ULL<<32)|42,
        .cr0_fixed0=0x80000001,.cr0_fixed1=~0ULL,
        .cr4_fixed0=1ULL<<13,.cr4_fixed1=~0ULL,.cr0=0x80000001};
    assert(!vmx_gate(&good));
    struct vmx_caps c=good;
#define CHECK(field,value,reason) do { c=good;c.field=(value);assert(!strcmp(vmx_gate(&c),reason)); } while(0)
    CHECK(feature_control,0,"FEATURE_CONTROL_UNLOCKED");
    CHECK(feature_control,1,"FIRMWARE_DISABLED");
    CHECK(basic,good.basic | (1ULL<<31),"INVALID_BASIC");
    CHECK(basic,6ULL<<50,"INVALID_BASIC");
    CHECK(basic,(6ULL<<50)|(4097ULL<<32),"INVALID_BASIC");
    CHECK(basic,(4096ULL<<32),"UNSUPPORTED_MEMORY_TYPE");
    CHECK(cr4,1ULL<<13,"ALREADY_VMXE");
    CHECK(cr0,1,"CR0_FIXED_BITS");
    CHECK(cr0_fixed1,1,"CR0_FIXED_BITS");
    CHECK(cr4_fixed1,0,"CR4_FIXED_BITS");
    CHECK(cr4_fixed0,(1ULL<<13)|(1ULL<<20),"CR4_FIXED_BITS");
    c=good;c.basic|=1ULL<<48;assert(!vmx_gate(&c));
    return 0;
}
