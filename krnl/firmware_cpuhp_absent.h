#ifndef SYLE_FIRMWARE_CPUHP_ABSENT_H
#define SYLE_FIRMWARE_CPUHP_ABSENT_H
#include <stdbool.h>
#include <stdint.h>
/* Exact discovery-only absent CPU-hotplug device probe. No CPU/device state,
 * interrupts, hotplug support, host IO or authorization to change CPU count.
 * Accept OUT32 0xaf00 zero, OUT8 0xaf05 zero, IN32 0xaf00 -> 0xffffffff.
 * Values mean the architectural operand (EAX/AL), not unused upper RAX bits.
 * IN32 zero-extends; OUT leaves RAX untouched. Every rejection leaves RAX
 * untouched. Caller must separately enforce one actual boot vCPU, fw_cfg
 * CPU-count consistency, and stage architectural commit/fail-stop behavior. */
bool fcha_io(uint16_t port,unsigned width,bool input,uint64_t *rax);
#endif
