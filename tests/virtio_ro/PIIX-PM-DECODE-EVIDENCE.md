# PIIX4 PM decode correction before native diagnostic

The initial projected profile imposed command.IO AND PMIOSE. That was a
conservative extra restriction, not a verified chipset requirement. It is now
removed from this optional profile; the original default fpp was never changed.

[QEMU v9.2.0 hw/acpi/piix4.c](https://raw.githubusercontent.com/qemu/qemu/v9.2.0/hw/acpi/piix4.c),
reviewed2026-11-13: the pm_io_space_update function selects the PMBA range and uses
PMREGMISC bit0 for enable, independent of PCI command. pm_write_config
updates that mapping for40/80 changes. piix4_pm_reset writes PMBA low byte1
and PMREGMISC0. Therefore forcing command reset1 is unnecessary for this model.
This is evidence for a deliberately selected QEMU-style subset, not a claim
about every physical PIIX revision or the outer QEMU11.1.3 implementation.

Matching upstream EDK2 2025.02 source archive already used by the firmware work:
`OvmfPkg/Library/AcpiTimerLib/BaseRomAcpiTimerLib.c` lines72–82 programs PMBA then
PMIOSE, without writing PCI command. `PlatformInitLib/Platform.c`386–401 sets
command.IO only in the branch where PMIOSE was initially zero. Its earlier
constructor path therefore explains why a mandatory extra command gate blocks.
This is source analysis, not proof of every library dispatch in the pinned FD.

Independent scan of run24 logged config-address samples found308 total,
PMBA75, PMREGMISC33, command04 zero. Exact serial hash and scope are recorded in
`evidence/piix-pm-run24-trace.json`. Lack of a logged sample does not prove the
absence of all possible unlogged accesses.

Host regression now checks PMBA=B001/PMIOSE1 with command0 really reads the actual
fpm timer; clearing command.IO alone keeps legacy decode; clearing PMIOSE stops;
PMIOSE0/command1 stops. No PCI storage command.IO/BUS_MASTER gate was relaxed,
and no target RAM callback, SCI/SMI or native DMA was enabled. Command register read/write
projection behavior remains explicit in the register table.

## Pinned binary and run24 combined check

The run24 build log records the checked CODE/VARS files and their embedding into
fo-rom.S. Independently hashed local pinned CODE/VARS copies match the original
package hashes624e06de18b4fa535e90db7160d00d3d07d206422b89999bf1e27d920264e4e0
and5d2ac383371b408398accee7ec27c8c09ea5b74a0de0ceea6513388b15be5d1e.
Run24 artifact manifest records outer kernel
da2e5a22de2d4c4064a6d3c435f796b8281e410e9ac14573fa9a164929e3b156.
This ties the trace to the recorded pinned build; the archived ELF is not locally
available for an independent extraction of its embedded FD in this check.

`verify_pm_trace.py` parsed all308 complete logged PCI-cycle/PM-state records.
Chronological first observations: DID word1237 at index2; PMIOSE byte0 at6;
PMBA dword1 at10; writeB001 at11; PMIOSE write1 at16; B008 dword timer read at30.
All28 logged timer reads were accepted with PMBA B001/enable1 and returned the
reported timer ticks modulo24 bits. The DID read RIPfffd2848 in the original
trace corresponds to bytes66ed in the pinned CODE flash (word IN AX,DX).
Machine-readable summary: evidence/piix-pm-run24-sequence.json.

This establishes actual observed DID/enable/base/port behavior for this run,
not merely an upstream default. It does not identify which exact linked library
instance cached a port or prove later cache validity. Upstream BaseRom/Dxe source
only supplies an interpretation; no module identity or long-lived cache claim is
inferred from these I/O samples.
