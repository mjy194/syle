# Limited PCI configuration projection contract (host candidate)

This is a separate opt-in configuration image, not default fpp/fpi behavior and
not full i440fx/PIIX emulation. Three present function profiles are proposed:
00:00.0 retains8086:1237 class0600 rev2;00:01.0 diagnostic PIIX4 identity8086:7110
class0601 rev0/header80;00:01.3 diagnostic PM identity8086:7113 class0680 rev0.
The PIIX identity values are platform choices here, not observed hardware or
proof all chipset-specific drivers are implemented. The matching EDK2 2025.02
I440FxPiix4.h confirms host ID1237 and PM BDF00:01.3/PMBA40/PMREGMISC80, but does
not establish fn0/PM IDs or complete driver compatibility. Native exposure waits
for additional source/actual-driver review. Virtio00:02.0 remains existing vrt.

All reads: aligned byte/word/dword within256 bytes. Each byte derives from this
register table, not an arbitrary writable array. Absent BDF returns not-routed.
Writes return failure without changing state for rejected access/side effects.

| Offset | Reads | Writes / mask / effect |
|---|---|---|
|00..03|fixed vendor/device|RO ignored|
|04..05|command: host/ISA0; PM bit0|host/ISA RO0; PM RW IO bit0 only, mask others0|
|06..07|status0|all implemented status bits0; PCI W1C errors maskF900 accepted as no-op; no error latches/capabilities advertised|
|08..0b|fixed rev/progif/subclass/class|RO ignored|
|0c|cacheline0|RO0 ignored: no cacheline behavior advertised|
|0d|latency0|RO0 ignored: no bus mastering|
|0e|type0, fn0 multifunction80|RO ignored|
|0f|BIST0|RO ignored: no BIST support|
|10..27|unimplemented BARs0|writes including probes ignored, stay0|
|28..3f|all0 incl ROM/capabilities/interrupt|RO/unimplemented ignored|
|40..43 PM only|current live fpm.pmba|byte/word/dword merge into live value then exact fpm_config validation (only1 orB001; enabled change rejected)|
|80 PM only|current live fpm.enable|byte or aligned wider write merge; exact fpm_config bit0 validation; adjacent reserved bytes must remain0|
|remaining40..ff|reserved0|zero writes no-op; any nonzero unsupported chipset write rejects|

Mixed dword04 stages command and status together. No error event source exists,
so status W1C writes never set bits. PMBA and enable are never duplicated in the
projection: callers provide the actual fpp.pm, staged on a private copy for
validation, committed once on success. PM command.IO is projected but does not gate the fixed legacy PM ports.
`vpc_pm_decode` follows the selected QEMU PIIX4 model: live PMIOSE plus
admitted B000 PMBA. See PIIX-PM-DECODE-EVIDENCE.md for the source correction.

The D000..DFFF block BAR window remains a diagnostic resource restriction;
normal OVMF allocation may choose elsewhere and must then fail, not relocate
silently. Complete256-byte reads do not imply compatible writes or native boot.
No interrupt/SCI/SMI/reset/power-management mechanism is added. Default paths
and native mapping refusal stay unchanged.

Implementation: `pci_config_projection.{c,h}` plus explicit
`vfb_init_projected` in the host bus wrapper. Original `vfb_init` retains the
old board configuration view. `pci_config_projection.sh` checks the table and
partial write atomicity; `pci_enumeration.sh` performs a normal function0-first
multifunction scan and full-width cross-checks, reaching BDF0/8/11/16 exactly.
`firmware_bus_disk.sh` runs the real firmware RAM/block combination under BOTH
old and projected profiles. PM command defaults0; PMIOSE can still enable the fixed timer before PCI
enumeration. No existing native behavior is claimed preserved solely from this
host regression; the profile remains opt-in and unbooted.

Tests do not execute an OVMF resource allocator or chipset driver. A successful
host scan cannot prove the currently pinned FD will accept these identities,
write only supported chipset registers or choose the D000 aperture.
