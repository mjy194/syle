#ifndef SYLE_FIRMWARE_TPM_ABSENT_H
#define SYLE_FIRMWARE_TPM_ABSENT_H
#include <stdbool.h>
#include <stdint.h>
/* Explicit absent-TPM probe, NOT TPM emulation/security functionality.
 * Matching EDK2 TisPcPresenceCheck/Tpm12TisPcPresenceCheck read Access byte;
 * Tpm2IsPtpPresence explicitly interprets 0xff at base as "No TPM chip".
 * Only observed locality0 base 0xfed40000, byte READ, returns logical value ff.
 * No writes, other addresses/widths, commands, measurement, sealing, attestation,
 * keys, IRQ, DMA, host TPM access, or security guarantees. Stateless host-only.
 * False leaves output untouched; success replaces logical load value, NOT a
 * whole guest GPR. Caller must prove decoded operand/GLA/GPA/EPT, owned fetch,
 * register merge and architectural transaction separately. This helper itself performs no native access. */
bool fta_mmio(uint64_t gpa,unsigned width,bool write,uint64_t *value);
#endif
