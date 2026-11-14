# Scoped memory access proposal (design only)

## Existing raw map cannot meet revocation

sg_memory_ops.map returns a raw pointer. Checking a pin when producing it does
not track its later use. No borrow counter can make an escaped raw pointer
revocable. Do not label current malloc fixtures/native direct map as scoped
leases or implement a pretend unmap that merely decrements a count.

The smallest enforceable API direction is **bounded operations, no returned
pointer**. A new opt-in memory-access table should offer copy_to_owned,
copy_from_owned, zero_owned and typed little-endian scalar loads/stores. Each
operation takes an internal allocation capability (issuer/slot/generation),
expected immutable role, offset, length and epoch; it returns a result, never
an owned-page address. The adapter alone translates/maps and performs the
copy. No caller callback receives the internal pointer. Host source/destination
spans must also be validated for length/overflow/alias with authority metadata
and backing storage. Do not use a public PA as the ownership credential.

For a minimal first integration, replace LGM image/params copies and lvh region
zero/revision writes with these bounded operations under separate role-aware
v2 entrypoints. EPT access then uses typed 64-bit operations tied to EPT_TABLE
capabilities. The default map ABI can remain unchanged, but the scoped path
must not silently fall back to raw map. Actual virtio guest-GPA copy translates
through separately validated EPT/RAM ownership; its input is not an arbitrary
allocator capability supplied by the guest.

## Record and state transitions

Private controller-owned records contain epoch, allocation generation, kind,
lifetime pin handle, OPEN/CLOSING/DEAD phase, active borrow count and a retained
failure code. There is one actual authority pin per page, compatible with the
current single-lease PMM. Borrow counters are subreferences to that pin, not
additional independently forgeable PMM leases.

- Admission under the same owner synchronization: verify epoch/generation/kind,
  bounds without overflow, OPEN phase, and actual authority pin. Reject count
  saturation. Increment borrow before obtaining any host pointer.
- Perform the bounded operation without publishing a pointer. Its code must
  not call arbitrary guest/user callbacks or recursively enter the controller.
- Finish under the same synchronization: release the internal pointer scope,
  publish required memory ordering, decrement exactly once. An ambiguous copy
  outcome retains an error result and forbids transaction retry, even though
  its borrow still must be correctly accounted for.
- Begin close atomically changes OPEN to CLOSING. New operations fail. Existing
  operations may finish; a nonzero count blocks unpin/free. Do not spin waiting
  for a same-CPU interrupted borrow. Return retained/busy to management or
  fail-stop on an impossible ownership state, never pretend the count is zero.
- After genuine VMX/device/global exclusion and count zero, unpin and release
  each allocation. A partial free failure keeps the exact record and prevents
  new CREATE. DEAD follows actual release; epoch end follows all DEAD records.

Logical revocation here means refusing new bounded operations and waiting for
already-admitted operations. It does not forcibly invalidate pointers. Existing
raw-map users must be proven retired before this property applies. Physical
host aliases, EPT mappings and other-device mappings are separate audit/isolation
requirements; a counter alone cannot revoke those aliases.

## Synchronization and NMI

A plain bool busy, IF=0, or single BSP is insufficient for NMI. Do not put a
spinlock around the current code and call it NMI safe: an NMI interrupting its
holder can deadlock. For the initial opt-in design, all borrow/close entrypoints
must reject NMI context before locking, and the actual NMI/exception path must
be audited not to access these buffers or invoke teardown. A reliable per-CPU
NMI-depth/entry marker requires assembly/interrupt integration and is not
presently demonstrated. Unknown context or exception during a copy must take
the existing fail-stop path while retaining ownership, not recover by dropping
pins. AP access must be excluded or governed by a real multicore protocol.

If NMI access is required later, design an NMI-safe reference scheme separately
with proven linearization/overflow/ABA and nonblocking reclamation. Do not
borrow that claim from host unit tests. Device DMA additionally requires device
quiescence/IOMMU ownership and completion semantics; MFENCE is only ordering,
not revocation or isolation.

## Concrete next host tests, not native proof

- Pure transition tests: count saturation, stale epoch/generation, wrong role,
  close racing admission at each serialized boundary, duplicate finish token,
  partial release, interrupted copy failure and owner-exit retention.
- Controlled in-operation close attempt with real guest_control: rejects
  reentry/retains ownership; no pin removed while borrow active.
- Actual PMM authority pin plus bounded copy primitives against host backing,
  overflow/alias/first-last-byte checks and no pointer-returning API surface.
- Actual LGM/lvh v2 users with no map fallback (set map callback fatal in test),
  full-capacity bootstrap and all failure unwinds.
- Separate native paging/NMI/exception audit before claiming native readiness.

No native scoped API or NMI marker is implemented. The independent synchronous
pilot below does not change the limits of the host lifecycle fixture.

## Independent synchronous pilot implemented

`scoped_access.c/h` and `scoped_access.sh` now exercise a bounded copy API with
actual PMM pins. Public operations return no owned-page pointer. Admission uses
epoch, full allocation capability, immutable role and offset/length checks;
zero length, cross-page spans and overflow attempts are rejected. The pilot
handles one page per operation, not a multi-page DMA transaction. Its fixture
backend receives an internal PA and copies synchronously to/from an explicitly
allowed external buffer. The backend is trusted and must not retain addresses,
start asynchronous work or hide incomplete execution behind a return value.

The host tests use fixed backing arrays, no unmapping, no concurrent cores and
no NMI. The borrow counter/busy fields have NO native synchronization. They
prove sequential state transitions only: closing blocks new calls, an admitted
copy may finish, and retire refuses an in-flight borrow. Actual paging validity
throughout the operation, TOCTOU exclusion, NMI context detection, and mapped
backing lifetime are still native blockers. No caller-supplied `nmi=false` is
accepted as a substitute for those mechanisms.

Tests cover wrong type/stale generation, UINT_MAX borrow saturation, last-byte
access, cross-page and SIZE_MAX offsets, unauthorized host-buffer rejection,
close attempted during transfer, actual pin preventing physical release, and
partial-copy failure latching faulted/no retry while retaining owner until
close. There is no raw-map fallback and no LGM/lvh integration of this API yet.
Out/config storage and authority metadata remain trusted/disjoint lifetime
inputs, not guest-writable memory. Release requires a separate proven global
quiescence path; its function name/precondition is not itself proof of one.

Additional regressions now run in four separate sanitizer processes. The
host_buffer callback attempts recursive copy/allocate/retire and is refused;
it may start drain, but the already-counted operation is allowed to finish its
transfer. Drain is admission revocation, not cancellation of an existing copy.
A mismatched epoch performs no transfer. Actual PMM extent exhaustion makes
physical release fail after unpin: the owner record/epoch remain live with
unchanged allocation count, and coalescing extents permits retry to finish.
Callbacks must not mutate ops, authority, tokens, pins or borrow counters;
those are trusted private adapter state, not hostile plugin inputs.

## Fatal interruption versus completed copy failure

Two additional host tests interrupt host_buffer/transfer with a nonreturning
fatal stand-in. They observe borrower=1, busy=true and the actual PMM pin/live
owner retained, then EXIT without cleanup or resuming the operation. Longjmp
is only a host observation mechanism, not a recovery path. These tests do not
execute NMI. The actual guest NMI observations elsewhere interrupted CPL3 wait
loops, not a live scoped copy; do not conflate the two evidence sets.

A backend returning false promises synchronous completion (possibly partial
copy), so the operation can account for its borrow while latching no-retry.
A fault/NMI with unknown continuation must not fake that return or drop its
borrow/pin in an error-finally block. Fail-stop retains resources. A marker
read cannot atomically cancel a copy or exclude later NMI; admission plus
nonreturning interruption semantics and exclusion must be separately proven.

## Both sides of the copy need lifetime authority

The owned allocation pin alone does not keep a caller's external source or
destination buffer resident/stable. The current host fixture allows a fixed
array; native admission would need a corresponding trusted buffer lifetime,
permission and alias proof. A conservative first profile can use permanently
resident kernel-owned input/scratch objects with exact compiled bounds, never
an arbitrary pointer plus a caller's 'trusted' flag. Dynamic buffers need their
own scoped reference and close ordering. Immutable boot-image sources and a
private fixed scratch buffer are materially different from unpinned user or
plugin-supplied addresses.

Existing direct-map aliases remain mapped after logical scope end. The bounded
API prevents exporting a new owned pointer; it does not make preexisting raw
pointers disappear or prevent unrelated kernel code from dereferencing the
same PA. All consumers on an activated profile must use the new API or have a
separate proven lifetime, with no fallback to sg_memory_ops.map. Current legacy
LGM/VMX paths still use raw maps, so native activation remains blocked.
