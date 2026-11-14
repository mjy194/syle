# Typed allocation and per-CREATE PA context experiment

This is an isolated interface implementation and host test, NOT installed
sg_memory_ops callbacks. It returns physical identifiers and pins but never maps a
host pointer, copies guest data, changes EPT or resumes a vCPU. Real DMA remains
blocked by mapping/exclusion/lifecycle integration.

## Type source and capability binding

The optional PMM authority now records immutable allocation purpose in the same
record as owner/generation/lease: UNSPECIFIED, GUEST_RAM, EPT_TABLE, VMX_CONTROL or
HOST_METADATA. A typed record is created only from a fresh allocator result;
there is no adopt/relabel API. Queries require the issuing allocation-generation
capability. Old untyped allocate remains UNSPECIFIED and cannot pass RAM admission.

`vtc_pin_ram` queries the actual authority record and exact allocation generation,
not just a type label in the per-CREATE inventory. Tests mutate the context's EPT
label to RAM and still fail pin admission. VMXON and VMCS allocations both have
VMX_CONTROL purpose and are explicitly tested as rejected for RAM pinning.
This does NOT prove physical semantic purpose from hardware: the trusted native
allocation call site must choose the correct purpose. Guest/external metadata
must never choose it. Additive LGM/VMX role-aware entry points now make explicit role requests, and
host integration tests connect them to authority. The default native manager
still uses its existing untyped callbacks; these APIs alone are not native binding.

The full optional registry now budgets 32,837 records: 32,768 RAM + 67 EPT + two
VMX control pages. At 40 bytes/record plus bookkeeping it consumes about 1.25 MiB.
This covers those known allocations only; additional host buffers need separately
reserved capacity. The optional final ELF has been relinked after this change.

## Exact per-CREATE context

`create_context.c` keeps PA→generation/type entries in caller-provided fresh,
stable storage. The context and entry array must be disjoint, externally owned
and not recycled while any old reference exists. Open issues one new authority
session epoch and is allowed only once per context. New CREATE uses new storage
and a new epoch. Every typed allocation adds its authority token before exposing
the PA, so later PA callbacks do not invent a capability from an address.

The experiment forbids re-exposing a retired PA within one CREATE. Tombstones
retain prior addresses. If the PMM returns the same address again, the new token
is released through authority and the request visibly fails without changing the
caller's output. This is intentionally restrictive: a normal allocator often
returns its lowest free page immediately, so repeated allocate/free churn may
keep failing even with free RAM. There is no spin/retry or claim of general
malloc-compatible behavior. Existing lgm_prepare allocates its fixed RAM/EPT set
before teardown; binding to that exact phase requires explicit integration and
producer tests. For workloads that legitimately reuse PA during a CREATE, change
the callback ABI to carry generation, or defer physical reclamation until epoch
end. Do not silently remove tombstones.

The inventory's upper bound is caller capacity. Accepted allocations consume one
slot for the context lifetime, even after release. A failed duplicate allocation
whose rollback succeeds consumes no additional slot or PMM page. Capacity is
checked before allocating. Actual PMM tests verify count/accounting behavior and
show old-context operations reject the same PA allocated in the next epoch.

## Failure and cleanup semantics

A successful authority allocation followed by lookup failure is retained as a
quarantined capability even if PA was never published. Failed duplicate rollback
also retains the new token, marks the context failed and rejects subsequent PA
release/pin/allocation operations. Cleanup uses stored generation tokens, not a
stale PA. Failure of `release` MUST leave the allocation intact; this is the real
PMM contract, unlike a DMA write that can have partial effects.

Close begins drain and tries only unpinned records. Pinned or failed releases
retain exact entries and prevent end. Already released entries remain retired,
so retries do not double-free. The holder can unpin an existing lease during
drain and retry close. The caller must establish the real stopped-guest and
mapping-lifetime conditions before close; this module does not do so itself.
Fresh empty epoch creation happens only after all owned allocations are released.

## Tests

`native_candidate.sh` now runs:
- Actual PMM/candidate host tests: RAM pin allowed; EPT and both VMX controls
  rejected; context relabel does not bypass authority; capacity failure leaves
  counters unchanged; partial close retains pinned RAM while releasing others;
  unpin/retry cleanup succeeds; old context rejects newly reused PA; within-epoch
  reuse fails with successful rollback and unchanged output/accounting.
- Failure-only mock tests: post-allocation lookup failure and duplicate rollback
  failure quarantine the token; repeated close failure retains it; later cleanup
  succeeds without exposing a PA-only release shortcut.

Neither test installs a mapping adapter or validates actual VMX/DMA concurrency.
The no-LTO final ELF proof still does not call these context APIs at runtime.
