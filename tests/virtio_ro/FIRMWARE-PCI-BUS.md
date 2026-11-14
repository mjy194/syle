# Single-latch PCI/virtio composition, host pilot

`firmware_pci_bus.{c,h}` is an independent optional wrapper, NOT native linked.
Its only configuration-address latch is the actual `fpp_state.pci.address`.
Existing CF8, hostbridge/absent CFC, PIIX PM config/timer, A20 and hotplug accesses
are delegated unchanged to actual `fpp_io`. Enabled CFC selection00:02.0 alone
routes to the actual legacy virtio transport. Other bus/function addresses stay
with the existing board model. No second CF8 latch or PCI passthrough exists.

BAR0 is64-byte I/O, with probing/unassigned supported and assignment restricted
to D000..DFC0 inclusive. This is a new exclusive board reservation in this pilot,
not an inference about arbitrary machines: native composition must reserve this
window against any other registered device. It excludes existing PM B000,
hotplug AF00, CF8/CFC, fw_cfg, UART, PIC/PIT/RTC and A20. Decode requires command.IO
and a non-probing assigned BAR. Relocation disables the old aperture; unexpected
port accesses fail terminally under the existing strict-board contract. DMA
still requires BUS_MASTER and the backend's polling/single-head profile.

Instruction admission reuses actual fpp_encoding, checks privilege/DX/canonical
RIP, samples the actual PM clock and stages the disk/GPR state until RIP commit.
A notify may already have written guest data/status/used.idx before RIP write
fails. In that case ALL later bus calls are refused; no replay/rollback is
attempted. The owning controller must actually stop the VM and retain resources.
A busy latch rejects synchronous callback reentry but is not an atomic lock or
NMI/concurrency proof. Direct callbacks or direct fpp/vrt calls bypass this
wrapper and are forbidden by the trusted controller contract.

`firmware_pci_bus.sh` strict ASan/UBSan links real board/PM/transport/backend.
It checks hostbridge and block IDs, subword RAX semantics, disabled enable bit,
absent BDF, same-latch PIIX registers/timer after disk access, BAR probe/relocation,
reserved-window rejection, disabled decode, actual virtqueue completion,
RIP failure after published used.idx, failed BAR commit, stale reset ring,
misaligned CFC, CPL rejection and busy terminal gate. Publication is a host
counter, memory is a fixture; `firmware_disk.sh` separately validates the real
firmware RAM copier. Neither alone or together proves native DMA.

Remaining before guest storage: complete present PCI header/multifunction
PIIX enumeration (existing board deliberately stops many such reads), actual
firmware VM-exit controller integration/terminal shutdown, validated owned RAM
lifetime and publication fence, immutable independent disk backing, interrupt
and multi-head semantics, and separately configured PCI/virtio-enabled Linux.
No default source, Linux config, frozen firmware kernel or native mapping gate
is changed by this work.

Follow-on combined test: `firmware_bus_disk.sh` now links actual firmware-memory
prepare/copier, adapter, this bus, actual board/PM and block backend in one host
process. CF8 IDs/PIIX/config/notify and actual map-fault partial writes are no
longer covered only in separate fixtures. Full enumeration is still absent.
Clock sampling occurs before RIP commit and may persist on failure; staging is
limited to configuration/device metadata and GPR publication, not all state or
irreversible guest memory writes. The terminal contract is essential.
