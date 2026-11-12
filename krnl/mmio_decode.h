#ifndef SYLE_MMIO_DECODE_H
#define SYLE_MMIO_DECODE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Restricted long-mode decoder. Only MOV r32 <-> m32 (89 /r, 8b /r),
 * optional single REX.R/X/B, address size 64. No REX.W or other prefixes.
 * No LA57: both instruction range and four-byte EA must be 48-bit canonical.
 * Registers use architectural encoding order:
 * RAX RCX RDX RBX RSP RBP RSI RDI R8 R9 R10 R11 R12 R13 R14 R15.
 * False means unsupported/invalid; *out is unchanged. Caller performs the
 * 32-bit store or zero-extending register load, not this decoder.
 * Effective-address overflow and four-byte accesses crossing 4 KiB reject.
 * Base-less SIB disp32 is signed and may denote high canonical addresses. */
struct mmio_mov32 {
    uint64_t ea;
    uint8_t length, reg;
    bool write;
};
bool mmio_decode_mov32(const uint8_t *code, size_t available, uint64_t rip,
                       const uint64_t gpr[16], struct mmio_mov32 *out);
#endif
