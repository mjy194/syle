#ifndef SYLE_GUEST_MMIO_MOV_H
#define SYLE_GUEST_MMIO_MOV_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Strict 64-bit-code-mode MOV planner. It neither reads guest memory nor
 * changes registers. GPR order: RAX RCX RDX RBX RSP RBP RSI RDI R8..R15.
 * Supported 88/89/8A/8B,C6/C7 /0, optional single 66/67 before optional REX.
 * Segment overrides, REP, LOCK, repeated/misordered prefixes and all other
 * opcodes reject. 67 means 32-bit effective-address arithmetic in long mode.
 * 48-bit canonical only, no LA57; memory operand must fit one 4 KiB page.
 * Caller must confirm long code mode, zero DS/ES/SS bases, supported device
 * width, and independently walk/validate GLA->GPA against EPT exit evidence.
 * Never trust VM-exit instruction length for EPT. Available bytes must have
 * been read safely from guest instruction mapping. False leaves output intact. */
struct guest_mmio_mov {
    uint64_t linear, next_rip, store_value;
    uint8_t length, width, reg, reg_shift;
    bool write, immediate, address32;
};
bool guest_mmio_mov_decode(const uint8_t *,size_t available,uint64_t rip,
                          const uint64_t gpr[16],struct guest_mmio_mov *);
/* Build the destination GPR value for a validated load. Caller commits only
 * after full MMIO transaction succeeds. Preserves AH/16-bit partial writes;
 * 32-bit writes zero extend. False leaves output unchanged. */
bool guest_mmio_mov_load_value(const struct guest_mmio_mov *,uint64_t old_reg,
                              uint64_t loaded,uint64_t *new_reg);
#endif
