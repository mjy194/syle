#ifndef SYLE_NESTED_PROFILE_H
#define SYLE_NESTED_PROFILE_H
#include "nested_vmx.h"
#include <stddef.h>
struct nested_field_value { uint64_t field,value; };
struct nested_profile {
    uint64_t cr0,cr4,efer,pat;
    uint64_t l2_entry,l2_resume,l1_exit;
    bool nested_ept;
    /* Restricted two-owner domain: CR3 is GPA; descriptor/stack bases stay GVA. */
    bool shared_domain;
    bool domain_contexts;
    bool domain_eviction;
    bool domain_atomic_ad;
    bool domain_atomic_catalog;
    bool domain_typed_uc;
    bool domain_typed_contexts;
    bool domain_typed_dynamic;
    /* Optional trusted immutable root catalog, opt-in AD0 dynamic domain.
     * This callback only admits exact full keys; no guest pointers/ownership. */
    uint64_t domain_initial_eptp;
    bool (*domain_root_allowed)(void *,uint64_t);
    void *domain_root_context;
    unsigned domain_cpu;
    bool ept_map;
    bool ept_sparse;
    bool ept_contexts;
    bool ept_dynamic;
    bool ept_large;
    bool ept_ad;
    bool ept_ad_modes;
    bool memory_types;
    bool uc_contexts;
    bool uc_pat;
    unsigned physical_bits;
    bool state_roundtrip;
    bool entry_failure_probe;
    bool exception_routing;
    bool entry_injection;
    bool gp_injection;
    bool pf_injection;
    bool pf_routing;
    uint64_t l2_gp_fault;
    uint64_t l2_finish;
};
static inline bool nested_state_save_controls(uint64_t entry,uint64_t exit,uint64_t secondary) {
    const uint64_t save=(1u<<2)|(1u<<18)|(1u<<20);
    const uint64_t restore=(1u<<2)|(1u<<14)|(1u<<15);
    return (exit&save)==save&&(entry&restore)==restore&&!(secondary&(1u<<2));
}
enum nested_profile_status { NP_VALID,NP_INVALID_CONTRACT,NP_CONTROLS,NP_HOST,NP_GUEST,NP_EXPECTED_GUEST_FAILURE };
/* Trusted monitor constructs this table; L1 may use it as bootstrap input to
 * real VMWRITE. Addresses are low guest addresses, never L0 host addresses. */
size_t nested_profile_table(const struct nested_profile *p,struct nested_field_value *out,size_t capacity);
unsigned nested_early_failure(const struct nvmx *s,const struct nested_profile *p);
enum nested_profile_status nested_profile_check(const struct nvmx *s,const struct nested_profile *p);
#endif
