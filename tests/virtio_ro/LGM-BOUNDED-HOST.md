# Actual LGM bounded-operation host integration, isolated candidate

Run `sh tests/virtio_ro/lgm_bounded_host.sh`. This generates a separate copy of
actual linux_guest_memory.c and source-classified linux_boot.c. Shared/default
sources are untouched. Exact anchored transformations preserve allocation/WB,
boot loading, ownership arrays, publication and release code; only pointer-based
operations become zero/write/u64 callbacks. No fake map pointer is used. The
candidate no longer requires map and a fail-fast map callback proves none is
called. EPT PD access is identified by PA/capability on every write, not a pointer
retained across mapping another PT.

The fixture uses the real optional PMM authority and typed generation tokens.
It validates every target's current epoch/token/PA/kind and pins it for each
synchronous operation, unpinning only after the operation has completed. Every
operation is page-bounded. The byte storage is a host heap with the old fixture's
fixed contiguous PA indexing assumptions, NOT paging_phys_to_virt or a native
mapper. Raw pointers exist inside this private fixture operation, not returned
to LGM. There is no actual hardware consumer of EPT/VMX memory. These short pins
therefore do not claim to replace the lifetime pins needed before any VM entry.

Input kernel/initrd bytes are private file buffers whose hashes are checked,
and cmd is a private fixed array; all remain alive and immutable for both
inspections and loading. Generated bp/q sources are synchronously consumed via
private scratch. Classification is from a trusted generated producer, not a
capability offered to arbitrary callers. No concurrent source mutation or alias
is injected; native source residency, acquisition and freeze remain unproved.

The normal process runs original actual LGM then releases it and starts a new
PMM epoch at the same PAs. Candidate RAM plus all 67 EPT pages (32835 pages)
match the original bytes in full. Both allocator epochs drain/end cleanly.
There are 68792 bounded operations for the checked kernel/initrd inputs.

Failure processes inject a current-operation partial write/zero (three bytes,
or one for a shorter operation), latch faulted, return false after known
synchronous completion, and reject all further target operations. Selected
positions cover initial/last RAM zero, first/last EPT zero, early boot loading,
last boot operation, first EPT store and final EPT store. These are SELECTED
positions, not exhaustive 68792 fault injection. The earlier source-only test
covers all 86 transfers of its smaller synthetic image, before each chunk.

Every failure leaves prepared=false/eptp=0. Actual LGM rollback releases all
owners except a deliberately failed first-page release; that exact page remains
recorded with actual PMM allocated count 1. Clearing the release failure permits
actual LGM retry to clear ownership and end the epoch. Partial bytes are never
restored or used to restart a guest. No guest has started in any test. Fatal or
unknown completion is not simulated by returning false; it must not unpin or
run this recoverable rollback protocol.

No native mapper, NMI exclusion, external DMA, boot, hardware VMX quiescence or
concurrent guest-byte snapshot proof follows. Native map remains blocked. This
candidate is not linked into default LGM or session manager. A production
controller still must bind immutable operations/source identity, lifecycle and
non-reentry, pin hardware references, freeze input bytes and revoke all other
raw consumers before activation.

### Review: remaining adapter gaps

This fixture is not yet the complete scoped-access implementation. In
cb_page_write, source span checks and generated bp/q copying into scratch occur
before operation acquires the target pin/counts its borrow. It is safe only in
the fixture's single-threaded, callback-free, fixed-source context; it does not
meet the intended production contract that admission/access share the protected
scope. Integration must move these into a controller-bound scope covering both
source and destination, not merely attach a target-page pin afterwards.

The LGM seam still passes PA; the fixed sidecar resolves a current allocation
capability and checks it with the authority. That validates the current record,
not the identity of an arbitrary stale caller that knows the same PA. Sequential
epoch reuse in this test does not prove stale-caller rejection. A production
operation handle must bind session/context/epoch before resolving PA, and old
callbacks must be revoked. No candidate native activation is justified here.
