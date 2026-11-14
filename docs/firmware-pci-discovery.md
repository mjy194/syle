# Bounded PCI configuration-discovery model

Selected fixed OVMF first reads CF8 as a DWORD. This fixture implements PCI
configuration mechanism1 only. It is a simulated device, not host passthrough,
and not a claim of complete i440fx/PIIX4 compatibility.

CF8 is a reset-zero 32-bit latch with mask0x80fffffc; only32-bit accesses work.
CFC..CFF accept naturally aligned byte/word/DWORD accesses without crossing
CFF. Disabled cycles or BDF other than00:00.0 read width-sized all-ones and
ignore writes because no device decodes the transaction. There is no invented
PIIX4, ACPI PM function, storage, display, BAR or DMA engine.

At00:00.0, offsets00..0f are explicitly modeled read-only: Intel8086:1237
identity, revision2, host-bridge class060000, type0 header; command/status,
cache-line size, latency and BIST reset zero. Command zero means no IO/memory
or bus-master decode is enabled by this fixture. This is an i440fx-compatible
identity discovery surface, not an emulation of PAM/SMRAM/DRAM registers.
Any access outside these modeled bytes, and ALL present-device config writes,
stops observation. Do not acknowledge an unsupported write as successful.

IN8/16 preserve high RAX bits; IN32 zero-extends. OUT consumes low data bits.
String/REP and illegal/reserved I/O qualifications fail without mutation.
RIP advances only on handled exit; unsupported requests keep the faulting RIP.

Matching OVMF PlatformInitLib/Platform.c selects the PIIX4 PM route on DID1237:
PCI command, PMBA(offset40), PMREGMISC(offset80), IO decode at0xb000 and PM timer
at+8. PlatformInitLib/MemDetect.c also uses platform identity in RAM/MMIO
layout. Those dependencies remain absent and must be separately modeled,
tested and reviewed; returning the host-bridge ID does not satisfy them.
The current EPT low-RAM/flash map also is not a complete legacy PAM/SMRAM map.

QEMU v10.0.0 hw/pci-host/i440fx.c independently confirms vendor/device identity,
revision2, host-bridge class and the need for separate PAM/SMRAM side effects.
No QEMU config-write implementation or host device is invoked here.

Acceptance: pure ASan/UBSan tests for latch masks, lane/width behavior, absent
cycles, all modeled header reads, unsupported writes, qualification rejection
and unchanged state on failure; independent review; tiny regression followed
by bounded OVMF observation. Diagnostic cleanup35 remains NOT boot success.
