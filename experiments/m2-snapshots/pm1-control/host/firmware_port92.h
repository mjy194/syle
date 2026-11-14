#ifndef SYLE_FIRMWARE_PORT92_H
#define SYLE_FIRMWARE_PORT92_H
#include <stdbool.h>
#include <stdint.h>
/* Fixed A20-enabled VMX guest contract; no bit20 wrap, reset service, A20
 * disable, device state or host IO. Supports the firmware IoOr8(0x92,BIT1)
 * sequence only: IN8 reports 0x02; OUT8 admits exactly AL=0x02 idempotently.
 * Read preserves upper56 RAX bits; output uses AL, ignoring unused high bits.
 * Every rejection preserves RAX. Native caller must stage architectural
 * commit and fail-stop on partial failure; this pure model cannot prove VMX
 * mode/admission or establish the backing memory's addressing semantics. */
bool fp92_io(uint16_t port,unsigned width,bool input,uint64_t *rax);
#endif
