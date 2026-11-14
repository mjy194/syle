# Owned-memory / bus routing adapter pilot

`platform.c` is host-only interface scaffolding. It does **not** consult the Syle
allocator, VM/EPT ownership tables, actual PCI config dispatch or native VMX IO
completion. All runtime-source files remain unchanged. Tests exercise synthetic
RAM and bus transactions, not DMA isolation on hardware.

## Trust boundary

The platform supplies stable, pinned ordinary-RAM allocation records, unique
owner ID, R/W permissions, immutable disk backing and a complete reserved-port
map. These are **trusted attestations**, not proof of ownership by themselves.
Never obtain these records from guest memory. Adapter checks owner/type equality,
range arithmetic, guest-range overlap, host-backing overlap, disk overlap and
adapter-state overlap; it cannot prove that the supplied host pointer is actually
allocated/pinned, excludes other VMs/page tables/code, or is not being changed by
another worker. Those guarantees need real allocator/EPT APIs before integration.
No span may cross two records, even contiguous ones. Copies perform no allocation
or remapping, eliminating the prototype's own late mapping failure, but not the
platform's lifetime obligations. `barrier` is a **host test counter**, not a hardware
DMA fence; this adapter must not be connected to a live guest as-is.

Initialize into its final stable address: copying an initialized adapter would
leave callbacks pointing at the old object. Initialization inputs must be external
to its storage; do not reinitialize an active object. Reset/teardown requires
exclusive stopped-guest ownership, no callbacks in flight and prior revocation
of all old accesses. There is no concurrency/refcount/allocator implementation.

## Transaction contract

- Caller routes one BDF through `vrp_config`; other BDFs return unhandled. The
  real bus supplies absent-device reads and routes all other devices.
- `vrp_io` returns unhandled for disabled decode, probe state, unassigned BAR or
  outside aperture. Old ports after relocation are unhandled without poisoning
  device state. A decoded invalid register or transaction is terminal.
- Before assigning a BAR, adapter checks all supplied reserved-port ranges.
  Conflict aborts before the BAR is changed. The board must provide **all**
  reservations, including other devices, and update the exclusion decision
  atomically with its real IO dispatcher. This pilot has no dynamic bus allocator.
- PFN setup requires avail.idx and used.idx both zero; nonzero rejects before
  activation without wiping guest RAM. This is an explicit fresh-queue subset:
  a valid producer pre-populating avail before PFN setup would be rejected and
  needs a deliberate compatibility change, not silent data loss. Full allocation
  read/write ownership is subsequently checked by backend initialization.
- A successful notify may already have published used.idx. Caller keeps the
  guest stopped through the enclosing instruction/RIP commit. If commit outcome
  is uncertain or fails, call `vrp_abort`, stop the VM and never retry that notify.
  Abort revokes adapter DMA access and permanently marks transport/backend failed;
  it does not roll back memory or pretend a published request did not happen.
- Status-zero device reset retains PCI routing and clears backend state, but does
  not reset producer RAM. A stale queue fails the fresh-queue check on re-enable.
  A terminal adapter failure cannot be cleared by guest reset.

## Verification

`sh tests/virtio_ro/run.sh` builds all three layers with strict freestanding C11
and exercises host tests under ASan/UBSan (LSan disabled for traced executor).
Adapter tests cover decoded successful IN, abort/no-retry after publication,
unhandled other BDF/ports, BAR conflict preserving old base, probe, relocation,
old-port exclusion, disabled IO, stale initial/reset queue, wrong owner/non-RAM,
host alias, disk/state alias and read-only ring rejection. No actual PCI bus,
allocation ownership proof, DMA fence, guest sharing, or OVMF/Linux acceptance is
claimed. Independent review is required before extending this prototype.

Permanent review regression additions: reserved-port ranges touching either side
without overlap; highest 0xffc0 BAR; conflict at final port 0xffff; duplicate GPA
with distinct host backing; cross-record span rejection; abort revokes admit,
read and write callbacks without changing either source/destination sentinel.
Initialization inputs must be external and object inactive; these are caller
API preconditions, not runtime-detected protection against invalid aliases.
