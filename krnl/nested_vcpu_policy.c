#include "nested_vcpu.h"
static bool address(uint64_t p)
{return p && !(p&4095) && p<(UINT64_C(1)<<52);}
static unsigned slot(const struct nested_vcpu *v,uint64_t vmcs)
{
    if(!v || !vmcs)return 0;
    if(vmcs==v->vmcs01)return 1;
    if(vmcs==v->vmcs02)return 2;
    return 0;
}
bool nested_vcpu_init(struct nested_vcpu *v,uint32_t owner,uint64_t vmcs)
{
    if(!v || !address(vmcs))return false;
    *v=(struct nested_vcpu){.owner_apic=owner,.vmcs01=vmcs,.cleared01=true};
    return true;
}
bool nested_vcpu_register02(struct nested_vcpu *v,uint32_t owner,uint64_t vmcs)
{
    if(!v || !address(v->vmcs01) || owner!=v->owner_apic || v->busy || v->poison ||
       v->vmcs02 || !address(vmcs) || vmcs==v->vmcs01)return false;
    v->vmcs02=vmcs;v->cleared02=true;return true;
}
bool nested_vcpu_authorize(struct nested_vcpu *v,uint32_t owner,uint64_t vmcs,bool resume)
{
    unsigned s=slot(v,vmcs);
    if(!s || owner!=v->owner_apic || v->busy || v->poison || v->entries==UINT64_MAX ||
       resume!=(s==1?v->launched01:v->launched02))return false;
    v->busy=true;v->pending_vmcs=vmcs;
    if(s==1)v->cleared01=false;else v->cleared02=false;
    return true;
}
bool nested_vcpu_complete(struct nested_vcpu *v,uint32_t owner,uint64_t vmcs,
    unsigned status,uint64_t reason)
{
    unsigned s=slot(v,vmcs);
    if(!s || owner!=v->owner_apic || !v->busy || v->pending_vmcs!=vmcs)return false;
    v->busy=false;v->pending_vmcs=0;v->last_status=status;v->last_reason=reason;
    if(status || (reason&(UINT64_C(1)<<31)) || v->poison || v->entries==UINT64_MAX){
        v->poison=true;return false;
    }
    if(s==1)v->launched01=true;else v->launched02=true;
    ++v->entries;return true;
}
bool nested_vcpu_clear(struct nested_vcpu *v,uint32_t owner,uint64_t vmcs)
{
    unsigned s=slot(v,vmcs);
    if(!s || owner!=v->owner_apic || v->busy || (s==1?v->cleared01:v->cleared02))return false;
    if(s==1){v->launched01=false;v->cleared01=true;}
    else {v->launched02=false;v->cleared02=true;}
    return true;
}
