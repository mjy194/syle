#include "../nested_vcpu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define REJECT(call) do {struct nested_vcpu old=v;assert(!(call));assert(!memcmp(&v,&old,sizeof v));}while(0)
int main(void)
{
    struct nested_vcpu v={0};
    REJECT(nested_vcpu_init(&v,2,0));REJECT(nested_vcpu_init(&v,2,0x1001));
    REJECT(nested_vcpu_init(&v,2,1ull<<52));assert(!nested_vcpu_init(0,2,0x1000));
    assert(nested_vcpu_init(&v,2,0x1000));assert(v.cleared01&&!v.busy&&!v.poison&&!v.entries);
    REJECT(nested_vcpu_register02(&v,3,0x2000));REJECT(nested_vcpu_register02(&v,2,0x1000));
    REJECT(nested_vcpu_register02(&v,2,0x2001));assert(nested_vcpu_register02(&v,2,0x2000));
    REJECT(nested_vcpu_register02(&v,2,0x3000));
    REJECT(nested_vcpu_authorize(&v,3,0x1000,false));REJECT(nested_vcpu_authorize(&v,2,0x3000,false));
    REJECT(nested_vcpu_authorize(&v,2,0x1000,true));
    REJECT(nested_vcpu_complete(&v,2,0x1000,0,18));
    assert(nested_vcpu_authorize(&v,2,0x1000,false));assert(v.busy&&!v.cleared01&&v.pending_vmcs==0x1000);
    REJECT(nested_vcpu_authorize(&v,2,0x2000,false));REJECT(nested_vcpu_clear(&v,2,0x1000));
    REJECT(nested_vcpu_complete(&v,3,0x1000,0,18));REJECT(nested_vcpu_complete(&v,2,0x2000,0,18));
    assert(nested_vcpu_complete(&v,2,0x1000,0,18));assert(v.launched01&&!v.launched02&&!v.busy&&v.entries==1);
    REJECT(nested_vcpu_complete(&v,2,0x1000,0,18));REJECT(nested_vcpu_authorize(&v,2,0x1000,false));
    assert(nested_vcpu_authorize(&v,2,0x1000,true));assert(nested_vcpu_complete(&v,2,0x1000,0,10));
    assert(v.entries==2);
    assert(nested_vcpu_authorize(&v,2,0x2000,false));assert(nested_vcpu_complete(&v,2,0x2000,0,18));
    assert(v.launched01&&v.launched02&&v.entries==3);
    REJECT(nested_vcpu_clear(&v,3,0x1000));REJECT(nested_vcpu_clear(&v,2,0x3000));
    assert(nested_vcpu_clear(&v,2,0x1000));assert(!v.launched01&&v.launched02&&v.cleared01);
    REJECT(nested_vcpu_clear(&v,2,0x1000));REJECT(nested_vcpu_authorize(&v,2,0x1000,true));
    assert(nested_vcpu_authorize(&v,2,0x1000,false));assert(nested_vcpu_complete(&v,2,0x1000,0,18));
    /* VMfail and VM-entry-failure do not establish launch state. */
    for(unsigned status=0;status<4;status++){
        assert(nested_vcpu_init(&v,2,0x1000));assert(nested_vcpu_authorize(&v,2,0x1000,false));
        assert(!nested_vcpu_complete(&v,2,0x1000,status,status?0:(1ull<<31)|33));
        assert(!v.busy&&v.poison&&!v.launched01&&!v.entries&&!v.cleared01);
        REJECT(nested_vcpu_authorize(&v,2,0x1000,false));REJECT(nested_vcpu_register02(&v,2,0x2000));
        assert(nested_vcpu_clear(&v,2,0x1000));assert(v.poison&&v.cleared01);
    }
    /* A failed VMRESUME preserves established launch state until VMCLEAR. */
    assert(nested_vcpu_init(&v,2,0x1000));assert(nested_vcpu_authorize(&v,2,0x1000,false));
    assert(nested_vcpu_complete(&v,2,0x1000,0,18));assert(nested_vcpu_authorize(&v,2,0x1000,true));
    assert(!nested_vcpu_complete(&v,2,0x1000,2,0));assert(v.launched01&&v.entries==1&&v.poison);
    assert(nested_vcpu_clear(&v,2,0x1000));assert(!v.launched01&&v.poison);
    assert(nested_vcpu_init(&v,2,0x1000));v.entries=UINT64_MAX;
    REJECT(nested_vcpu_authorize(&v,2,0x1000,false));
    assert(!nested_vcpu_authorize(0,2,0x1000,false));assert(!nested_vcpu_complete(0,2,0x1000,0,18));
    assert(!nested_vcpu_clear(0,2,0x1000));assert(!nested_vcpu_register02(0,2,0x2000));
    puts("nested_vcpu_policy: PASS");
}
