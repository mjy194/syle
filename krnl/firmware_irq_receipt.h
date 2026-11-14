#ifndef SYLE_FIRMWARE_IRQ_RECEIPT_H
#define SYLE_FIRMWARE_IRQ_RECEIPT_H
#include <stdbool.h>
#include <stdint.h>
/* Pure host model, NOT a provider/authenticator of native evidence.
 * Caller binds immutable identities to selected VMCS and actual entry attempt.
 * 'accepted' means injected external IRQ crossed vector-delivery phase, NOT
 * that its handler ran or completed. Never infer receipt from entry-info alone.
 * No VMCS writes/accounting/cancellation or retry is performed here. */
struct fir_identity {uint64_t domain,vmcs,attempt;};
struct fir_evidence {
 struct fir_identity observed;
 uint32_t staged_info,native_status,reason,vectoring,exit_interruption;
 bool run_reported_exit,host_restored,profile_admitted;
};
enum fir_result {FIR_UNKNOWN,FIR_NO_ENTRY,FIR_ACCEPTED};
/* Only real native exit status1 is classified. VMfail/early host failures stay
 * UNKNOWN here: their definite-no-entry path belongs to separately bound logic.
 * profile_admitted excludes unsupported UINTR/PT/TAPT and async forwarding.
 * Valid-bit-clear vectoring payload is undefined and deliberately ignored. */
enum fir_result fir_classify(const struct fir_identity*,const struct fir_evidence*);
#endif
