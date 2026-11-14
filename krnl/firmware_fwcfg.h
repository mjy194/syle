#ifndef SYLE_FIRMWARE_FWCFG_H
#define SYLE_FIRMWARE_FWCFG_H
#include <stdbool.h>
#include <stdint.h>
/* Minimal read-only legacy fw_cfg probe, NOT a complete QEMU platform.
 * Optional FFC_NO_KERNEL_BLOBS adds only LE32 zero size keys17/8/b/14,
 * declaring absent external setup/kernel/initrd/cmdline payloads, NOT a boot
 * image or file service. Data/address/unknown/DMA remain rejected. Default:
 * Six immutable built-in items; unknown keys STOP at caller, unlike QEMU zero streams; no file entries, e820, ACPI, DMA or RAM access.
 * Copyable private model contains no host/GPA pointers. Zero-init then init.
 * Owner stages model/GPR and publishes only after architectural commit; on
 * partial commit failure it must fail-stop, not retry against advanced cursor. */
struct ffc_state { uint64_t cursor; uint16_t selector; bool ready; };
/* Pass prepared memory.layout.ram.size and actual vCPU count, not unchecked
 * CPUID/firmware guesses. This profile admits exactly 128MiB and one vCPU. */
bool ffc_init(struct ffc_state*,uint64_t ram_size,unsigned cpu_count);
/* Selector OUT16 0x510 uses AX (little-endian IO operand); resets cursor.
 * Data IN8 0x511 preserves upper RAX and advances even for zero-filled tail.
 * Unknown/architecture/write-mode selectors, unsupported width/direction,
 * and DMA reject. This is intentionally stricter than QEMU's ignored writes.
 * False leaves state and RAX unchanged, including on cursor overflow.
 * State and RAX storage must be disjoint, trusted, serialized host objects. */
bool ffc_io(struct ffc_state*,uint16_t port,unsigned width,bool input,uint64_t *rax);
/* Non-string scalar VMX IO only. REP/string IO must use a separately reviewed
 * guest-memory transaction; this adapter deliberately rejects both. */
bool ffc_exit(struct ffc_state*,uint64_t qualification,uint64_t *rax);
#endif
