#ifndef SYLE_FIRMWARE_TPM_TRANSACTION_H
#define SYLE_FIRMWARE_TPM_TRANSACTION_H
#include "firmware_apic_transaction.h"
struct ftt_state { bool failed; };
/* Explicit Syle TPM-absent contract, NOT full QEMU TPM behavior/security.
 * Single serialized stopped vCPU; immutable owned RAM/CODE/page tables through
 * commit. Scalar MOV byte read only at FED40000; RSP/SPL destination rejected.
 * RFLAGS requires bit1, allows only arithmetic flags, IF/DF/RF/AC/ID;
 * TF/IOPL/NT/VM/VIF/VIP/reserved flags reject, as do debug/blocking/events.
 * RF completion clears only RF before
 * RIP (last VMWRITE); GPR merge publishes only after all writes succeed.
 * Rejection has no state/output effects but forbids resume. Partial VMWRITE
 * latches permanent fatal failure; never clear latch or retry. No native run,
 * device access, command, measurement, sealing, or attestation is performed. */
enum fat_result ftt_mmio(struct ftt_state*,uint64_t native_gpr[15],
 const struct fmm_memory*,const struct fat_exit*,bool exclusive,void*,
 bool(*write)(void*,uint32_t,uint64_t));
#endif
