#ifndef SYLE_FIRMWARE_IRQ_NATIVE_EVIDENCE_H
#define SYLE_FIRMWARE_IRQ_NATIVE_EVIDENCE_H
#include "firmware_irq_receipt.h"
#include "linux_vcpu_state.h"
/* Pure adapter only. Native caller MUST set entry_status=UINT32_MAX immediately
 * before the single lvs_run under exclusive actual VMCS ownership. Raw assembly
 * changes it to2 before host VMWRITE,0 before VMLAUNCH/RESUME,1 only at VMEXIT.
 * No VMREAD of stale exit fields is permitted on status0/2/sentinel paths.
 * Caller verifies actual profile/VMCS/host state; booleans are obligations. */
struct fine_outcome {
 struct fir_evidence evidence;
 enum lvs_result run_result;
 bool status_sentinel_set,exit_fields_read;
};
enum fir_result fine_classify(const struct fir_identity*,const struct fine_outcome*);
#endif
