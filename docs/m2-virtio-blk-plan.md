# M2 virtual storage: isolated implementation plan

Status: design/static audit and host-only model, 2026-11-13. No device, native boot or Linux
storage test is claimed by this document. This does not change the observer,
firmware candidate, direct-loader baseline, or default runtime path.

Parent coordination update: the live firmware branch is currently stopped at
early CFI flash behavior, before this storage/PCI phase. This plan is parallel
preparation, not the current boot blocker or evidence that CFI has been resolved.

## Audited starting point and target

Target: pinned Debian OVMF 2025.02-8+deb13u1 and separately configured Linux
6.12. Pin hashes of FD, kernel, config, raw disk and candidate for every run.
The companion preliminary `pci-virtio-next-contract.md` design records
matching upstream EDK2 transport/driver observations; those are source
observations, not proof those drivers dispatch in the pinned binary.

Current local code:

- `krnl/firmware_pci_probe.c:fpi_io`: CF8/CFC mechanism 1, absent BDF all-ones,
  only read-only first 16 bytes of i440fx 00:00.0. Present-header writes and
  later offsets fail. Not a general PCI bus or BAR allocator.
- `krnl/firmware_piix_pm.c:access`: separate 00:01.3 restricted PM configuration
  interception, no complete enumerable PIIX topology. A complete bus must retain
  this PM state, not replace it with a second independent configuration copy.
- `krnl/firmware_memory.c:ram_copy`: low 128 MiB RAM copy with owned backing;
  whole arithmetic range is checked, but mappings happen page by page and a
  later mapping failure can leave writes partial. No DMA transaction abstraction.
- `firmware_mmio_memory.h`: CPU virtual-address translation/instruction-exit
  checking. NOT a virtqueue DMA translation API; queue addresses are GPAs.
- `guest/linux-rebuild/config.fragment`: explicitly CONFIG_PCI=n. Preserve this
  independently verified direct-loader baseline; use a separate storage config.

## Chosen minimum device contract

A **legacy-only** virtio-blk device, initially single-vCPU synchronous service,
immutable host-owned memory-backed raw disk, no host block-device passthrough.
Do not advertise modern capabilities or call this a fully transitional device.

Proposed BDF 00:02.0: vendor 1af4, device 1001, revision 0, subsystem device 2,
subsystem vendor 1af4, class mass-storage/SCSI (01/00), Type 0 header. These are
virtual-platform choices requiring compatibility tests, not host discovery.
No PCI capabilities/MSI/MSI-X, no bridges. INTA# is reserved for the later
interrupt-capable stage; no claim that writing interrupt-line config routes IRQ.

BAR0: 64-byte I/O aperture, reset unassigned; dword size probe returns
0xffffffc1. All other BARs unimplemented (zero). Configuration supports legal
byte/word/dword accesses, read-only fields, masked command writes, W1C status
where implemented, BAR probing/restore/relocation and absent functions. The
bus model must define complete 256-byte headers for present devices. PIIX
function 3 also needs discoverable function 0/multifunction topology, or an
explicit alternative topology proven with OVMF. Do not invent bridges merely
to hide this gap. Reserve a nonoverlapping I/O allocation window; validate
against PIC/PIT/UART/RTC/CF8/fw_cfg/PM/hotplug ports and 16-bit wrap. BAR decode
requires PCI command.IO; queue DMA requires command.BUS_MASTER. Unsupported
BAR allocation fails explicitly instead of claiming an assigned but dead port.

Legacy BAR layout (no MSI-X, x86 little-endian):

| Offset | Width | Meaning |
| --- | --- | --- |
| 00 | 4 | device feature bits |
| 04 | 4 | guest feature bits |
| 08 | 4 | selected queue PFN, page size 4096 |
| 0c | 2 | queue size, 128 for queue 0, zero for other queues |
| 0e | 2 | queue select |
| 10 | 2 | queue notify, value 0 only for active queue |
| 12 | 1 | device status; zero resets queue/features/ISR |
| 13 | 1 | ISR status, read-to-clear |
| 14 | 8 | capacity in 512-byte sectors (individual legal-width reads) |

Initially advertise only VIRTIO_BLK_F_RO (bit 5). No indirect descriptors,
EVENT_IDX, FLUSH, topology, block-size override, MQ or VERSION_1. Physical
logical-block size is 512. Features unknown to the device cannot silently
be accepted. Legacy status sequencing must be checked against both pinned
clients: do not require the modern-only FEATURES_OK handshake. Honor FAILED;
notify before acknowledged DRIVER_OK/valid queue is rejected. Reset is exclusive,
clears pending interrupt and all queue ownership, and keeps the disk immutable.
PCI function reset semantics must not be confused with guest device status reset.

## Split ring and request engine

Queue 0 has 128 descriptors. Base = uint64(PFN)<<12, not a host pointer.
Descriptor table: 16*N bytes; avail: 4+2*N bytes; used starts at the next 4096
boundary after avail and is 4+8*N bytes (EVENT_IDX not negotiated). Check all
add/align arithmetic and full regions against admitted, owned DMA RAM. Do not
include optional event fields in functional semantics. Test exact layout against
OVMF and Linux producers rather than inferring from a successful allocation.

Maintain private last_avail and used indices, uint16 modulo semantics. Pending
avail delta must be <= N. Process bounded heads, max N descriptor visits, visited
bitmap, next<N, no loops/INDIRECT/unknown flags. Snapshot metadata and complete
request plan while the vCPU is stopped; no guest execution/concurrent worker
until commit ends. Validate every span with overflow-safe arithmetic and verify
all backing ownership before mutation. Never permit ROM, VARS, MMIO, host/EPT
pages, or another guest's memory. For the first strict profile reject data/status
alias with ring metadata and overlapping writable spans. These restrictions
must be reported as compatibility limits if a valid client hits them.

Normal request: readable 16-byte header (type/reserved/sector), zero or more
read-data writable descriptors, final writable status byte. Support scatter/gather
within the N bound; do not assume every read is three descriptors. Initially
require canonical header/status layout and nonzero sector-multiple read length;
validate this subset against actual clients. Add measured compatibility cases
rather than silently interpreting arbitrary scatter layouts. Bound total read
size by the immutable disk and ring representation; avoid a fixed small transfer
cap unless it is advertised through negotiated SIZE_MAX/SEG_MAX features.

IN(0): sector <= capacity and bytes/512 <= capacity-sector, then exact copy.
OUT(1): no disk mutation, status IOERR. GET_ID(8): implement a fixed 20-byte
identifier for Linux compatibility, with properly sized writable target.
FLUSH(4), DISCARD, WRITE_ZEROES and unknown requests: UNSUPP when a trustworthy
status target/chain permits completion. Disk-range failure: IOERR. Structural
memory/chain violations: terminal diagnostic, no unsafe attempt to find status.
Read-only filesystem writes must fail, not succeed as discarded writes.

Commit: data -> status -> used element(id=head,len=valid contiguous prefix of the concatenated
device-writable buffers) -> release barrier -> used.idx last. The length must never sum disjoint writes across unwritten holes. Error
completion with untouched read data before status reports len=0; oversized
GET_ID writes its first20 bytes and terminal status but reports len=20.
Exact20 GET_ID reports21; successful reads include terminal status. Update private ownership exactly once. Preflight
map/validate helps but is not rollback: any partial mapping/store failure is
terminal. Notification I/O RIP failure after a completed request must also be
terminal, never retry and double-complete. Inject failures at each boundary.
The current page-by-page RAM writer cannot alone prove atomic commit.

## Two distinct interrupt milestones

1. Firmware polling stage: only accept requests with avail.NO_INTERRUPT set;
   retain/diagnose any completion requiring an IRQ instead of silently dropping
   it. This can prove OVMF BlockIo polling only, NOT Linux boot support.
2. Linux stage: implement virtio legacy level-triggered INTx, completion ISR bit
   0 and read-to-clear deassertion, PCI command.INTX_DISABLE, masking/pending,
   and reset. Route one chosen PCI interrupt through a coherent PIRQ/PIC/IOAPIC
   and ACPI _PRT contract. Existing PIC support or interrupt-line byte alone is
   insufficient. Test shared-line/level behavior and OVMF-to-Linux transition.
   Linux must not be kept alive by an undocumented custom polling driver.

## Deliverable sequence and gates

A. Pure host PCI model + tests, no runtime integration. Cover existing i440fx and
PIIX probes, full enumeration, multifunction, legal access widths, BAR all-ones
probe, disable/relocate, overlap, reset. Decide resource/IRQ topology here.

B. Pure host queue engine + owned-RAM adapter + deterministic disk fixture.
Cover feature/status/reset, PFN overflow, unsupported queue, descriptor loops,
wrong direction, cross-page/nonowned/alias spans, ring index wrap, multiple heads,
last disk sector/end overflow, RO writes/GET_ID/unknown requests, and injected
mapping/commit failures. Compile with sanitizers where supported. Each test must
say host-only; no VMX success inference.

C. Separate opt-in firmware candidate, frozen evidence/hash and old fixture
regressions. First prove PCI enumeration and binding of the legacy transport and
VirtioBlkDxe in the pinned FD, then one known-sector read, then hash-checked ESP
file. Record actual BAR assignment, queue features/PFN/descriptors, and stage
markers; seeing vendor 1af4 in a log is not block-read proof.

D. Add/test IRQ path and separate Linux 6.12 recipe: CONFIG_PCI, VIRTIO,
VIRTIO_PCI, VIRTIO_PCI_LEGACY and VIRTIO_BLK built in; generated .config must
confirm symbols. Enable GPT/partition, EFI stub and relevant FAT/ext4 support
as required. Preserve original minimal recipe. Record exact source/toolchain and
image hashes. First boot test can use the known initramfs to verify device read
hash, write rejection, reset/reinit and IRQ counters, but label it intermediate.

E. Final storage/firmware gate: GPT raw disk with FAT ESP and a read-only Linux
rootfs, fallback EFI path EFI/BOOT/BOOTX64.EFI so NVRAM persistence is not a
prerequisite. Use a reproducible EFI boot payload (e.g. signed-off measured UKI
with kernel/initramfs/cmdline) and explicit root=PARTUUID, ro. Prove firmware
loads EFI bytes from the virtual disk, reaches kernel via EFI, mounts that root,
executes /sbin/init and reaches interactive shell. No host-injected kernel or
hidden direct loader. Keep /run and /tmp writable via tmpfs if needed. Test
cold repeat, STOP/START policy, no storage/IRQ use-after-free, destroy/recreate.

## Dependencies not solved by the storage model

OVMF PEI/DXE memory and platform progress; correct resource discovery and PCI
enumeration; IRQ routing and ACPI description; EFI handoff/memory-map support;
128 MiB memory budget for firmware + kernel + rootfs cache; independent GOP/
display and input path. Serial shell is useful evidence but does not silently
close the M2 display requirement. Current legacy-only fw_cfg does not supply
ramfb's DMA protocol. Immutable disk storage also needs explicit ownership and
lifetime separate from guest RAM; never release it until vCPU/device teardown.

The implementation can begin with A/B while OVMF progresses, but native stages
C/E remain blocked on firmware reaching discovery. No current runtime files were
changed by this plan. No tests of a live device have run.

## Host-only contract model evidence

`tests/models/virtio_blk_contract.py` is a Python executable specification, not
a PCI/device implementation. Its companion unittest file initially passed 14 self-tests, but independent
specification review found incorrect used-length expectations. Those expectations
and the model were corrected; the expanded suite now passes 19 tests in
the cloud workspace on 2026-11-13. Run with:

```sh
PYTHONDONTWRITEBYTECODE=1 python -m unittest discover -s tests/models -p 'test_virtio_blk_contract.py' -v
```

Covered: fixed split-ring layout, PFN/owned-region limits, unsigned span edges,
modular avail indices, read-only request classification, GET_ID, disk boundary,
used length overflow, descriptor cycle/flags/direction/aliases, cross-page
payload and scatter reads. Not covered: C arithmetic/port emulation, PCI
enumeration, feature/status state machine, guest-memory read/commit faults,
interrupt delivery, firmware binding or Linux device operation. This model
produces plans only and performs no DMA or disk access. It does not replace
the implementation and fault-injection gates above.


## Independent specification review and future C ABI

Reference: [VIRTIO 1.2, including legacy-interface sections](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html).
The original model's tests agreed with an incorrect implementation; passing tests
alone were not specification evidence. Independent review corrected:

- §2.7.8.2: used.len describes a valid written prefix, not the sum of scattered
  data and terminal status stores. The model now emits explicit written ranges
  plus a prefix length. GET_ID oversized buffer and failed-read cases regress
  the original error. Legacy §5.2.6.3 drivers ignoring used.len is not an excuse
  for an incorrect newly implemented device.
- §2.7.4.2: readable descriptors must precede writable descriptors. Enforce at
  chain validation and at the request API, even for unknown request types.
- §2.7.5.2: limit total chain bytes (header and status included) to 2^32;
  individual descriptor length is uint32. Indirect chains remain unsupported
  because the feature is not offered, not because indirect is inherently invalid.
- §2.7.2: `allocation()` admits the complete legacy page-rounded allocation,
  including reserved event words/padding; `layout()` separately reports active
  fields touched by this profile. For PFN=1/N=128 allocation is [0x1000,0x3000),
  while active used fields end at0x2404. No DMA use of padding/event fields is
  implied. The full-allocation admission is a conservative ownership policy.
- §5.2.6 request rules: RO writes are IOERR without disk mutation; unknown request
  types are UNSUPP only after structural validation. Header/status descriptor
  separation, zero-length rejection and alias rejection are our strict subset,
  not claims that every excluded descriptor layout violates the specification.

For C implementation, use explicit little-endian wire decode helpers and static
size checks: descriptor (u64 addr/u32 len/u16 flags/u16 next), request header
(u32 type/u32 reserved/u64 sector), used entry(u32 head/u32 valid_prefix).
Do not cast guest RAM to host structs or serialize Python dataclasses as an ABI.
The planner should return a bounded array of prevalidated WRITE_DATA/WRITE_STATUS
operations with GPA/length, completion status, head and valid-prefix length;
commit is a separate fallible transaction. Status values are OK=0, IOERR=1,
UNSUPP=2, while this Python specification uses readable names. Reject arithmetic
before narrowing to wire widths, and reproduce fixed golden test vectors in C
as well as cross-language differential tests. Independent normative review and
real OVMF/Linux producer traces remain required, not replaced by differential
agreement between two implementations.

Unimplemented even in this model: used-slot allocation, ownership across heads,
cross-request descriptor/data aliases, reset epochs, avail publication ordering,
feature/status/PCI state, interrupt obligations, physical backing alias detection,
DMA map failures and commit fault injection. The caller currently supplies one
immutable descriptor snapshot and admitted GPA regions. A new queue-ownership
state-machine model is needed before describing this as complete queue validation.
