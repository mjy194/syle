# Concrete PMM owner/lease interface experiment

Status: optional **host-only** link against the repository's real `krnl/memory.c`,
not a native safety feature. No existing kernel source or default build is edited.
This advances beyond caller-created owner labels but does not close all DMA gates.

## Source audit

`memory.h` explicitly requires bootstrap CPU, interrupts disabled, no
synchronization. `memory.c` tracks sorted eligible and available extents, plus
aggregate counters; there is no per-page owner, pin or generation. In
`pmm_free_page`, eligible-and-not-free means allocated. It cannot tell whether a
page belongs to another subsystem or a guest. All normal allocated pages, even
pinned by a separate wrapper, are otherwise freeable through this API.

Existing `linux_*` allocation callbacks use pmm_alloc_page/pmm_free_page directly,
while mapping calls paging_phys_to_virt. A wrapper only around LGM is insufficient:
other PMM clients would bypass it. The authority must live at the common PMM free
boundary, and mapping/EPT APIs must independently enforce sharing restrictions.

## Executable optional host link

`lease_model.c` specifies allocation-record and lease transitions. No `adopt(pa)`
API exists: a protected record is created only from a fresh allocator result.
A record associates allocation provenance, owner identity, allocation generation
and an active lease serial. Every new allocation receives a never-reused
generation. Pin, owner release and allocation-page lookup require the allocation
handle (issuer + slot + generation), never a bare (owner, PA). Every new lease
gets a fresh serial, and unpin/lease-page lookup require that lease handle.
Both token kinds are bound to their issuing manager. Same-owner/same-PA ABA is
therefore checked at allocation as well as lease lifetime. Serials never wrap. Only one simultaneous lease per
page is supported by this experiment. Draining rejects new allocations/pins but
retains existing leases until released; drained does not mean pages were freed.
Release failure retains the exact allocation record, and pinned pages cannot
be freed. Allocation-contract violations latch terminal failure without guessing
that it is safe to free a duplicate/invalid token.

`pmm_authority.c` uses this state with the **actual PMM allocator**, not mock
physical addresses. In the optional host build only, memory.c's free entry point
is compiled as `pmm_raw_free_page`, and the normal `pmm_free_page` symbol routes
through the authority. It rejects all protected pages, even after their lease is
un-pinned, until owner-authorized release succeeds. Ordinary unowned allocations
retain existing behavior. The owner value identifies a kernel-owned memory
session, but knowing a value cannot adopt an arbitrary existing page into the
registry: it must have been allocated through this API. This is kernel-internal
bookkeeping, not protection against malicious kernel callers.

The raw symbol remains externally linkable in this experiment: **compile-time
renaming is not native enforcement**. A production implementation must move raw
free into memory.c as a private static helper and audit every free/reinit path.
No external symbol, custom allocator, direct free-list edit or metadata reset may
bypass the authority. The same exclusion protects allocation, pin, owner release,
free, epoch updates and teardown. No changes have yet made that true in native
builds. There is no claim that merely linking this host wrapper secures a VM.

## Smallest concrete production interface proposal

Retain the ordinary API for unowned host clients. Add PMM-internal owned allocate,
pin, lease validation, unpin and owned release, with an opaque memory-session
handle and never-reused generation. Use the prototypes in `pmm_authority.h` as
host sketches, not final ABI: replace public owner integers with lifecycle-issued
session handles; issue sessions only to the actual guest-memory controller.

LGM's memory-op context would reference that session. Its allocate/release wrappers
use owned allocation/release, while audit/lease resolution ties GPA to real ledger
and EPT entries. Existing `lgm_prepare` and `lgm_release` return/failure ownership
semantics remain intact: failed release keeps its token; pinned release fails
without freeing. No default Linux profile changes. Managed teardown must begin
drain and wait for leases **before** calling lgm_release, since that function
clears prepared/eptp before attempting releases. Early release while DMA is active
would still be invalid even if individual frees fail safely.

The experiment has 32 records and a single lifetime/session authority; this is
not enough for the 128 MiB guest plus EPT tables. Production needs bounded capacity
for at least LGM_PAGE_COUNT + LGM_TABLE_COUNT and all other managed allocations,
reserved before acquiring PMM pages, with checked exhaustion and accounting.
Metadata storage itself must be reserved/owned and excluded from guest mappings.
No model reinitialization is allowed with live records or leases. Generation
storage cannot be reset while stale handles may survive. Multiple guests and
concurrent leases need a separately tested registry/session design.

## Remaining native blockers and failure handling

- Bootstrap single-CPU exclusion is the only concurrency assumption exercised.
  Before SMP/interrupt-enabled use, introduce a common lock/critical-section rule
  covering PMM and session registry, including all legacy allocation/free clients.
- The authority guards frees, NOT mapping aliases. Another EPT or host mapping may
  still expose the allocation. Real map ownership/sharing checks and complete
  needed EPT alias validation remain required; the per-page audit is insufficient.
- Pins prevent allocator reclamation only if every native path honors them. They
  do not stop vCPUs/DMA writers, update EPT atomically or perform INVEPT.
- Teardown requires guest/device exclusion, lease drain and existing VMCLEAR,
  INVEPT, VMXOFF lifecycle proofs. Partial request/store/RIP failures terminate;
  never republish/retry. Cleanup cannot free pinned pages or reset the registry.
- Invalid/duplicate allocator results are fatal contract violations. The model
  intentionally retains state rather than freeing a potentially shared token;
  operator recovery/native leak accounting is not implemented.
- A real WB publication fence is still absent from the connected adapter. No
  DMA callbacks have been manufactured from these records or lease sketches.

## Actual tests

`run.sh` runs six host test programs plus existing PMM compatibility modes under
strict compilation and ASan/UBSan:
- Model failures: wrong owner, untracked token, duplicate pin, stale/double unpin,
  allocation failure, duplicate/invalid allocator result, capacity and serial
  exhaustion, drain and release failure retaining the record.
- Actual PMM optional link: ordinary free works; normal pmm_free_page cannot free
  protected pinned OR unpinned pages; wrong-owner/untracked pin fails; stale lease
  fails after reuse; registry exhaustion leaves PMM counts unchanged; ordinary
  allocation still works at registry capacity; drain retains existing leases.
- Actual PMM free-extent exhaustion: owner release fails with PMM counts/record
  retained, page can still be pinned, normal free remains denied; after coalescing
  adjacent extents, owner release succeeds.
- Unmodified `krnl/tests/memory.c` linked through the optional wrapper: normal,
  fragmentation, zero-page and single-page modes all pass. This is a focused PMM
  compatibility test, not all Linux semantics or any native guest boot test.

## Review-driven ABA correction

The original host API accepted (owner, PA) for pin/release, which let stale callers
act on a newly allocated page at the same address. That API has been removed.
All protected-page operations now require an allocation-generation token or a
lease token; returned PA is diagnostic data, never an authorization token. Tests
reuse exactly the same PA for the same owner and reject stale release, pin and
query. They also cover foreign-manager tokens, double unpin, allocation/lease
serial exhaustion (without invoking the allocator), init refusal both while
active and after draining, and the real PMM's existing ready reinit guard.

Manager storage must remain at a stable address for its full handle lifetime;
manual zeroing/destruction followed by reuse would violate the API contract.
The host implementation permits init only once per zero-initialized object, and
the optional PMM authority has process-lifetime static storage. A future native
session restart needs a persistent epoch issuer and quiescence/revocation proof,
not memset/reset of live generation counters. No reboot-safe capabilities or
protection against malicious kernel writes is claimed.
