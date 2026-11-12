#ifndef SYLE_EPT_H
#define SYLE_EPT_H
#include <stdint.h>
#include <stdbool.h>
/* Minimal trusted guest diagnostic, not a general mapping API. */
#ifdef VMX_NESTED_DOMAIN_TEST
#define EPT_GUEST_PAGES 63
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
#define EPT_GUEST_PAGES 47
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
#define EPT_GUEST_PAGES 25
#elif defined(VMX_NESTED_EPT_TEST)
#define EPT_GUEST_PAGES 19
#elif defined(VMX_NESTED_TEST)
#define EPT_GUEST_PAGES 14
#elif defined(VMX_STACKPART_TEST)
#define EPT_GUEST_PAGES 13
#else
#define EPT_GUEST_PAGES 11
#endif
#define EPT_CODE_GPA UINT64_C(0x6000)
#define EPT_STACK_TOP UINT64_C(0x8000)
struct ept_fixture {
    uint64_t tables[4], pages[EPT_GUEST_PAGES], eptp, allocated_before;
    bool started;
};
static inline bool ept_caps_supported(uint64_t c) {
    const uint64_t need=(1ULL<<6)|(1ULL<<14)|(1ULL<<20)|(1ULL<<25);
    return (c&need)==need; /* walk4, WB, INVEPT, single-context INVEPT */
}
static inline bool ept_fault_matches(unsigned phase,uint64_t reason,
    uint64_t qualification,uint64_t gpa,uint64_t rip,uint64_t expected_rip) {
    const uint64_t addresses[3]={0x9000,0xa000,0xb000};
    const uint64_t access[3]={1,2|8,4|8};
    return phase<3 && reason==48 && gpa==addresses[phase] && rip==expected_rip &&
        (qualification&0x1ff)==(access[phase]|0x180);
}
static inline bool ept_demand_pte_valid(uint64_t pte) {
    return (pte & ~UINT64_C(0x20)) == UINT64_C(0x8003);
}
bool ept_fixture_demand_ok(const struct ept_fixture *f);
bool ept_fixture_movss_remediate(struct ept_fixture *f);
bool ept_fixture_movss_ok(const struct ept_fixture *f);
static inline bool ept_movss_fault_valid(uint64_t reason,uint64_t q,uint64_t gpa,
    uint64_t rip,uint64_t expected,uint64_t blocking) {
    return reason==48 && (q&0x1ff)==0x1aa && gpa==0xb080 &&
           rip==expected && blocking==2;
}
bool ept_fixture_idt_access(struct ept_fixture *f,bool allow);
bool ept_vectoring_gp_valid(uint64_t reason,uint64_t q,uint64_t gpa,
    uint64_t info,uint64_t error,uint64_t rip,uint64_t expected,
    uint32_t *entry_info,uint32_t *entry_error);
#ifdef VMX_STACKPART_TEST
bool ept_stackpart_protect(struct ept_fixture *f,bool protect);
int ept_stackpart_observe(const struct ept_fixture *f,uint64_t flags);
bool ept_stackpart_complete(const struct ept_fixture *f,uint64_t flags,uint64_t return_rip);
#endif
/* Fresh object only. BSP may prepare multiple private fixtures before VMX;
 * preparation does not execute INVEPT. Owner CPU must flush before first use.
 * Failed prepare releases only allocations belonging to this fixture. */
bool ept_fixture_prepare(struct ept_fixture *f);
/* Caller proves ALL relevant CPUs inactive/VMCS cleared, invalidation done,
 * and VMXOFF as required. No flush is performed. Frees only original owned
 * pages[]/tables[], never targets discovered in edited EPT leaves. Keep those
 * ownership arrays intact when mapping externally-owned shared backing.
 * Unlike legacy destroy, no global PMM allocation-count equality is assumed. */
void ept_fixture_owned_release(struct ept_fixture *f);
bool ept_fixture_init(struct ept_fixture *f);
bool ept_fixture_remediate(struct ept_fixture *f,unsigned phase);
bool ept_fixture_data_ok(const struct ept_fixture *f);
bool ept_fixture_flush(const struct ept_fixture *f);
void ept_guest_release(void);
void ept_fixture_destroy(struct ept_fixture *f); /* after inactive + INVEPT + VMXOFF */
extern const unsigned char ept_payload_start[],ept_payload_end[];
extern const unsigned char ept_normal_call[],ept_read_fault[],ept_write_fault[],ept_done_call[];
extern const unsigned char ept_exec_start[],ept_exec_end[];
#endif
