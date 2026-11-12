#ifndef SYLE_LINUX_PROFILE_GATE_H
#define SYLE_LINUX_PROFILE_GATE_H
#include "linux_apic_dispatch.h"
#include "linux_cpu_profile.h"
struct lgs_session;
struct lpg_choice {enum lcp_kind kind;uint64_t domain,initial_time;};
/* Read-only first-entry gate, not Linux acceptance. Caller serializes current
 * VMCS, owned EPT/RAM and fresh device domain from verification through entry.
 * Reads actual controls/EPT pages/firmware. No INSTALLED or caller-ready proof.
 * Output unchanged on failure; no silent fallback after partial preparation.
 * Does not select a live dispatcher profile or install firmware itself. */
bool lpg_prepare(const struct lad_config*,struct lgs_session*,const struct lvp_cpu_features*,struct lpg_choice*);
/* Per-entry controls/owned EPT check; no revalidation of mutable bootparams. */
bool lpg_runtime(const struct lad_config*,struct lgs_session*);
#endif
