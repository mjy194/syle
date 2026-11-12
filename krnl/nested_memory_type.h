#ifndef SYLE_NESTED_MEMORY_TYPE_H
#define SYLE_NESTED_MEMORY_TYPE_H
#include <stdbool.h>
#include <stdint.h>
enum nested_memory_type {
    NESTED_MEM_UC=0, NESTED_MEM_WC=1, NESTED_MEM_WT=4,
    NESTED_MEM_WP=5, NESTED_MEM_WB=6, NESTED_MEM_UC_MINUS=7
};
/* Encoding legality only, not hardware capability or alias-safety admission.
 * Arguments are decoded types, not raw EPTP/entry flag words. */
bool nested_memory_eptp_valid(uint8_t type);
bool nested_memory_leaf_valid(uint8_t type);
bool nested_memory_pat_type_valid(uint8_t type);
bool nested_memory_pat_valid(uint64_t whole_pat);
/* Model a single access with its selected guest PAT type.
 * First reject invalid leaf/PAT encodings, even when CD or IPAT would bypass
 * the PAT combination. Invalid input leaves output unchanged.
 * CD=1 gives UC; otherwise IPAT=1 uses leaf type; IPAT=0 uses Intel's table.
 * No host PAT/MTRR modification or inference of a guest page's PAT selection. */
bool nested_memory_effective(uint8_t leaf_type,uint8_t selected_pat,
    bool ignore_pat,bool cr0_cd,uint8_t *effective);
/* Conservative DATA-leaf admission when host backing is known WB:
 * leaf WB, CD=0, and every PAT byte legal; then IPAT=1 is sufficient.
 * IPAT=0 requires ALL EIGHT PAT entries WB, not just one guest-virtual alias.
 * This is intentionally stricter than encoding legality/effective-type model.
 * It does not validate EPTP table-page cacheability, CPU capability, backing
 * ownership, host MTRRs/PAT, or flush ordering. Those are separate gates.
 * No UC/WC/WT/WP host-backed execution is admitted by this helper. */
bool nested_memory_wb_leaf_safe(uint8_t leaf_type,bool ignore_pat,
    bool cr0_cd,uint64_t whole_pat);
/* Alias-independent compatibility with an externally established backing
 * type: all eight guest PAT selections must yield exactly that type.
 * CD=1 or NW=1 are deliberately unsupported, even if effective() can model
 * CD override. Legal final backing types are UC/WC/WT/WP/WB, not UC-minus.
 * This is pure type compatibility, not ownership/lease/transition admission.
 * Actual hardware callers must independently restrict allowed backing types. */
bool nested_memory_backing_compatible(uint8_t leaf_type,bool ignore_pat,
    bool cr0_cd,bool cr0_nw,uint64_t whole_pat,uint8_t backing_type);
/* Bounded N29 fixture progress only, not architectural INVEPT semantics.
 * PAT/CR safety is independently rechecked before every physical entry. */
static inline bool nested_uc_pat_cached_resume(unsigned completed,unsigned invalidations) {
    return completed==1 && invalidations==0;
}
#endif
