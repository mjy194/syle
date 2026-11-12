#include "cache_maintenance.h"
#include "cache_page.h"
#include "paging.h"
#include "vmx_memory.h"
static uint64_t pat(void){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0x277));return a|((uint64_t)d<<32);}
static void capture(struct cache_cpu_snapshot *s){uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));s->apic=b>>24;
    __asm__ volatile("mov %%cr0,%0":"=r"(s->cr0));__asm__ volatile("mov %%cr3,%0":"=r"(s->cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(s->cr4));s->pat=pat();
}
static bool readable(void *ctx,uint64_t p,const uint64_t **out){(void)ctx;
    if(!p||(p&4095)||p>=UINT64_C(0x100000000))return false;
    *out=paging_phys_to_virt(p);return *out!=0;
}
static bool stable(const struct cache_cpu_snapshot *s){if(!s)return false;uint64_t flags;struct cache_cpu_snapshot now={0};capture(&now);
    __asm__ volatile("pushfq;popq %0":"=r"(flags));
    return s&&s->mtrr_wb&&!(flags&0x200)&&now.apic==s->apic&&now.cr3==s->cr3&&now.cr3==paging_root()&&now.pat==s->pat&&
        !(now.cr0&UINT64_C(0x60000000))&&!(now.cr4&((1u<<7)|(1u<<12)|(1u<<17)));
}
bool cache_maintenance_precheck(uint64_t target,struct cache_cpu_snapshot *out){
    if(!out||!target||(target&4095)||target>=UINT64_C(0x100000000))return false;
    struct cache_cpu_snapshot s={.target=target};capture(&s);
    s.mtrr_wb=vmx_memory_region_wb(target)!=0;
    struct cache_page_lease lease;
    if(!stable(&s)||cache_page_audit(s.cr3,target,s.pat,8192,readable,0,&lease)!=CACHE_PAGE_AUDIT_OK)return false;
    *out=s;return true;
}
bool cache_maintenance_same(const struct cache_cpu_snapshot *a,const struct cache_cpu_snapshot *b){
    return a&&b&&a->mtrr_wb&&b->mtrr_wb&&a->apic!=b->apic&&a->cr3==b->cr3&&a->pat==b->pat&&a->target==b->target;
}
bool cache_maintenance_tlb(const struct cache_cpu_snapshot *s){if(!stable(s))return false;
    __asm__ volatile("invlpg (%0)"::"r"(s->target):"memory");return true;}
bool cache_maintenance_cache(const struct cache_cpu_snapshot *s){if(!stable(s))return false;
    __asm__ volatile("wbinvd":::"memory");return true;}

bool cache_maintenance_lease(const struct cache_cpu_snapshot *s,struct cache_page_lease *out){
    return stable(s)&&cache_page_audit(s->cr3,s->target,s->pat,8192,readable,0,out)==CACHE_PAGE_AUDIT_OK;
}

bool cache_maintenance_lease_current(const struct cache_cpu_snapshot *s,unsigned current_type,
                                    struct cache_page_lease *out){
    return stable(s)&&cache_page_audit_current(s->cr3,s->target,s->pat,current_type,
        8192,readable,0,out)==CACHE_PAGE_AUDIT_OK;
}

bool cache_maintenance_precheck_type(uint64_t target,unsigned requested,
    struct cache_cpu_snapshot *out,unsigned *slot,uint64_t *bits)
{
    if(!out||!slot||!bits||requested!=4||!target||(target&4095)||target>=UINT64_C(0x100000000))return false;
    struct cache_cpu_snapshot s={.target=target};capture(&s);
    unsigned selected;uint64_t encoded;
    if(!cache_page_pat_slot(s.pat,4,&selected,&encoded))return false;
    s.mtrr_wb=vmx_memory_region_wb(target)!=0;
    struct cache_page_lease lease;
    if(!stable(&s)||cache_page_audit_current_type(s.cr3,target,s.pat,6,selected,encoded,
        8192,readable,0,&lease)!=CACHE_PAGE_AUDIT_OK)return false;
    *out=s;*slot=selected;*bits=encoded;return true;
}
bool cache_maintenance_lease_current_type(const struct cache_cpu_snapshot *s,unsigned type,
    unsigned slot,uint64_t bits,struct cache_page_lease *out)
{
    return stable(s)&&cache_page_audit_current_type(s->cr3,s->target,s->pat,type,slot,bits,
        8192,readable,0,out)==CACHE_PAGE_AUDIT_OK;
}
