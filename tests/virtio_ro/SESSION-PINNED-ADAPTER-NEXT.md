# Optional pinned session adapter: next increment, not implemented

The reviewed lvh inline barrier is necessary but not sufficient: current
session release still calls legacy lvh_close. Do not flip the default manager
or present the lvh unit test as a connected pinned-session path.

## Keep default linkage/layout independent

Mirror the prepare seam with an internal trusted release-path object containing
one close function pointer and its fixed context. The common session release
keeps its current host_safe -> close -> platform.disable -> host_safe ->
lgm_release ordering. Legacy wrapper passes no path and refers only to legacy
close, so existing mock fixtures and default binaries need no new dependency.
An independent optional adapter supplies a fixed close thunk that invokes
lvh_close_with_barrier. Do not expose a general user-selected close callback.
Copy path values before callbacks; actual binding storage is immutable for the
controller lifetime. Null/invalid path must fail before changing state.

## Private binding rather than receipt-as-data

Use the controller's existing fixed session pointer, one-shot authority
context and epoch, plus an internal release-inflight phase. The close thunk
sets that phase only around the actual lvh close invocation. Its inline
barrier must verify this phase, expected session/context/epoch, host.active=false,
host.ready=false, no entry-inflight, no device/async producer, and no scoped
mapping borrower. Kernel caller guarantees lifetime and exclusivity; booleans
are not a defense against arbitrary in-kernel pointer writes or NMI.

Do not accept a caller-provided `quiesced` receipt or token whose generation
merely equals a caller's number. Actual VMXOFF/no-entry control flow must invoke
the barrier; exact epoch identity must be checked against allocator authority.
A callback that anyone can call directly is not a hardware receipt.

## Pin lifetime and failure cleanup

RAM/EPT and VMX control allocations are pinned before returning their PA from
the role allocator. Failure to pin retains the unpublished allocation for
safe later cleanup, rather than silently publishing an unpinned page. Native
WB/map validation and host/EPT alias checks remain separate requirements.

On barrier success, unpin only this context's recorded live leases. A partial
unpin failure keeps progress and returns false: no physical frees happen in
that invocation; later retry skips already-unpinned or already-freed entries.
When all relevant pins are released, lvh performs control frees, then session
restores CR4 and releases LGM pages, then allocator end. A pin does not block
other kernel code from directly dereferencing existing aliases; actual map
scope/no-escape and exclusion are still required.

Open failure before VMX entry and after partial successful VMX entry both need
coverage. Automatic legacy cleanup may be blocked by lifetime pins; optional
session release must then pass through the same barrier, not unpin in a generic
error handler. Quarantine alias/unknown PA still blocks freeing a published
page. Never prematurely call allocator end to break an ownership barrier.

## Integration evidence required

1. Actual guest controller + actual session release + actual lvh close, with
   mock VMX instructions and actual PMM leases; assert precise release order.
2. Invalid epoch/fixed-context mismatch/entry-inflight/borrower nonzero: no
   unpin or free; state retained. Direct reentry via controller gives EBUSY.
3. Fail each VMX close instruction, fail barrier before/after partial unpins,
   fail one control free and one RAM free, then REAP without double-release.
4. Actual LGM allocation/copy/EPT build with host checked maps; malformed input
   before any allocation and first-EPT map failure. Verify input refusal does
   not start a guest or discard the allocator epoch.
5. Default session/VMX tests and unchanged struct ABI; independent final ELF
   proof that includes optional objects but does not enable native selection.

Keep this adapter optional and host-tested. The current one-shot authority
context still requires fresh stable storage on each new CREATE, with explicit
budget/exhaustion; no reset/reuse of old callback identity is authorized.

## Implementation update: opt-in session close path

The internal release-path seam and `lgr_configure_barrier` now exist. The
sidecar fixes its close thunk, verifies the exact host-context pointer and
close-inflight phase, and invokes the configured callback only from actual
lvh close after active/ready are false. Legacy session release still calls
legacy close. Session structs and platform ops layout remain unchanged; the
new optional binding struct carries its own hook fields.

`session_close_barrier.sh` connects actual guest_control, session, lvh and PMM
leases. LGM is a fixture and VMX callbacks are mocked. Tests check VMXOFF
failure, callback epoch mismatch, partial unpin, one-control free failure,
allocator end retry, and disable(false) fatal-stop before RAM release/end.
The fatal test uses longjmp only to inspect retained state and then exits;
it never resumes a failed host or claims native CR4 validation. The separate
full-LGM test still mocks lvh, so these two combinations must not be described
as one fully connected actual-LGM/actual-lvh native test.

## Full host combination (test only)

`session_full_pinned.sh` now links actual guest_control, session, LGM, lvh,
boot parser and PMM authority together at full 128 MiB capacity. Input hashes
are gated. VMX instruction callbacks remain mocks and mappings are aligned
malloc buffers. Tests cover all-page lifetime pins, off/barrier/epoch-mismatch/
partial-unpin/RAM-free/end failure recovery, and first-EPT map failure whose
pinned partial preparation is later reclaimed through the close barrier.

This fixture indexes entries by `(PA-BASE)/4096`, relying on its constructed
contiguous and ordered PMM input. It is NOT a general native mapper. `held[]`
checks prevent acquiring a new fixture pointer without a pin; they cannot
revoke a previously returned pointer and there is no mapping borrower counter.
All-page pins therefore do NOT establish scoped mapping leases. The barrier
runs in a synchronous fixture with no actual NMI/AP/device/DMA producers.
Only VMX mocks supply the instruction results; no real VMX instruction, guest
entry, DMA transfer or hardware-memory visibility has been validated.

The final test source is frozen for independent review; no production manager
selection is authorized by these results. The manager remains the legacy path.
