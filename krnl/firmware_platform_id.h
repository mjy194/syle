#ifndef SYLE_FIRMWARE_PLATFORM_ID_H
#define SYLE_FIRMWARE_PLATFORM_ID_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Read-only virtual IA32_PLATFORM_ID for the exact synthetic Intel signature
 * 0x0600. Platform class0 occupies bits52:50; all other bits are zero.
 * No host RDMSR, writable state or write API. Caller separately validates
 * Intel vendor, actual RDMSR opcode, MSR interception and atomic guest commit.
 * False leaves output unchanged. This does not identify the physical CPU. */
bool fpid_read(uint32_t processor_signature,uint32_t msr,uint64_t *value);
struct fpid_state { bool failed; };
/* Caller supplies safely fetched immutable instruction bytes, proven vendor
 * and exclusive stopped-vCPU ownership. !exclusive never mutates anything.
 * With ownership held, every validation/VMCS failure permanently latches
 * failed. RDMSR only, exact two-byte 0f32, CPL0, exit31, RCX low32=17.
 * Only successful RIP commit publishes zero-extended EAX/EDX. Callback failure
 * may be partial; never clear failed or resume/retry afterward. Native register
 * array uses RAX slot0, RCX slot2, RDX slot3. No host MSR access. */
bool fpid_apply(struct fpid_state*,uint32_t signature,bool intel_vendor,
 uint64_t native_gpr[15],uint64_t rip,unsigned length,unsigned cpl,uint32_t reason,
 const uint8_t *instruction,size_t available,bool exclusive,void *ctx,
 bool (*write)(void*,uint32_t field,uint64_t value));
#endif
