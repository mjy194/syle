# N38A dynamic typed directory: pure bookkeeping

This module is a serialized, caller-owned directory for the next opt-in
registry transaction bridge. It performs no allocation, VMCS write, INVEPT,
cache maintenance, alias scan, or hardware access. It is not a completed
multi-context typed-memory implementation. Existing N37 runtime and legacy
registry lifecycle hooks remain independent.

## Ownership and identity

The caller provides stable, exclusively owned `td_record`, `td_operation`,
and `td_receipt` storage. A record may be freed/reused only after successful
`td_cancel` or `td_forget`. An operation may be reused after `td_end` and must
supply its saved nonce on every call. A receipt must survive the associated
record and tree release: it remains linked for the directory lifetime. This
first version does not compact receipts or guarantee progress under a fixed
receipt-storage budget. The bridge must reserve receipt storage before an
irreversible retirement; allocation failure leaves the tree retained.

A root identity contains domain ID, domain incarnation, monotonic root ID and
root incarnation, allocator lifetime, and owner. This implementation assigns
the root ID and root incarnation from the same never-wrapping serial within
the domain incarnation. Reusing an HPA or a metadata address gives a new
identity. Domain incarnation uniqueness and allocator-lifetime provenance
remain external obligations; `td_init` is only for a fresh object after all
users of any prior incarnation have stopped.

Three counters have distinct meanings:

- `version`: directory metadata changes, including pins/refills/history.
- `type_epoch`: the typed transition generation.
- `cache_generation`: a particular root's cache reset generation.

Stable references use `td_identity`, never an array index from a snapshot.
Full physical EPTP and root HPA are separately bound when attached. EPTP's
root address must equal HPA; this module treats low control bits as opaque.
The registry/profile must validate EPTP controls and physical capabilities.

## Candidate lifecycle

1. `td_begin` admits a mutation operation and returns its nonce.
2. `td_reserve` registers a candidate before root allocation/initialization.
3. `td_attach` records an owned root, including one whose initial INVEPT failed.
   Initial history is explicitly zero. Generation zero is allowed for that
   failed-initialization case; it cannot be published.
4. After successful sparse initialization/flush and callback/refill setup,
   `td_publish` makes the candidate live. Those actual actions are bridge
   obligations, not facts inferred from a nonzero generation.
5. Failure with owned resources calls `td_retain`. A candidate with no attached
   root may instead cancel after its allocator resources are safely reclaimed.

An operation cannot end while one of its candidates is still initializing.
Even after the directory closes, an already admitted operation may attach,
finish or retain its candidate. It must then end before sealing. No new
operation can enter after close. A retained candidate with zero aliases and
zero historical consumers still appears in the frozen snapshot.

This is not a lock implementation. Every call and every direct read of its
mutable metadata must be serialized by the caller. A short external lock can
protect these operations; it must not remain held across allocation, actual
INVEPT, hardware entry, scans, callbacks or drain waits. The operation token,
not a held lock, spans such work.

## VMCS, refill and residency

Pins represent actual VMCS references. New root pin precedes VMWRITE; old
root unpin follows only confirmed success. An uncertain switch retains both
pins. Refill registrations account for software sources that can reinstall
translations; their count is not a substitute for enumerating those sources.
The bridge must revoke every source before dropping registrations.

`td_resident` monotonically accumulates every CPU that may cache the root,
not only the current running owner. Cache reset changes cache generation but
does not clear historical residency or authorize shared backing release.
Runtime entry must be serialized against directory freeze and must establish
its possible residency before hardware use. This module does not kick CPUs
or establish quiescence.

## Retirement and durable receipts

`td_retire_begin` requires zero pins and refill registrations. It changes
state so publication, new pins, refill admission and residency updates are
rejected. Its ticket binds complete identity, full EPTP, HPA, cache generation,
history, type epoch and unique retirement nonce.

After each historical consumer actually invalidates the appropriate EPTP,
`td_retire_commit` accepts only an exact complete history mask and matching
ticket. The caller supplies a nonzero flush-operation serial and independent
receipt storage. The receipt durably records the retirement type epoch,
retirement nonce and exact commit version. The function links the durable receipt before permitting
`td_forget`. Wrong identity, old ticket, incomplete masks, missing storage or
repeated commits cannot authorize free. A retry after retaining creates a new
retirement nonce. Even zero-history candidates need a receipt and nonzero
flush serial, because safe reuse of their allocated EPTP/root memory is an
allocator/hardware obligation.

These are typed attestations, not authenticated hardware receipts. A caller
must not manufacture a mask or serial without executing the required work.
The guarded sparse destroy integration must invoke this commit after actual
successful invalidation and before *any* tree or metadata free. Commit failure
retains the tree. A local receipt covers only the CPU bits actually supplied;
if history includes another CPU, a single-owner flush is insufficient.

A receipt authorizes retirement of that root, never release of shared data
backing. Backing reclamation still needs the complete domain transaction.

## Freeze and snapshot

`td_close` blocks new mutators/refills. `td_seal` requires all admitted
operations to have ended; only then does it produce a full freeze ticket.
The immutable ticket binds domain/incarnation, nonce, directory version and
type epoch. Snapshot reads require that ticket and have all-or-nothing output
on insufficient capacity. They include all remaining live/retained/retiring
records and persistent historical receipts. A retired-but-not-forgotten record
and its receipt can both occur; `receipt` distinguishes them, so a consumer
must not treat this as two physical roots requiring a freed-memory scan.

Only allocated live records may be physically scanned. Receipts cover roots
already safely retired; dereferencing their former HPA could access a new
allocation. Root identity, not HPA equality, determines correspondence.
Snapshot order is not stable and must never become a token identity.

`td_reopen` consumes the ticket and requires a strictly newer type epoch. It
only changes directory metadata. Before calling it, the coordinator must have
finished every real alias/refill/owner invalidation and cache-type transition,
prepared all durable lease tokens, and kept consumer entry closed. DTL must
use explicit stable-ID-to-snapshot-index translation, not reinitialize an old
ledger to silently rebind existing tokens. The current module neither edits
DTL nor grants hardware entry.

Poison rejects new work and all publication. Already nonconstructing operation
tokens can end, but poison is intentionally not an automatic cleanup/recovery
API. Failure can leave owned records and outstanding work retained forever.
It does not prove that another owner stopped, permit rollback or allow free.

## Pure verification

`typed_directory_test.c` covers history on both CPUs, pin/refill retirement
blocking, receipt allocation failure, incomplete/stale/duplicate retirement
receipts, same-address/HPA ABA, unknown owner, candidate initial-flush failure
with generation zero, close versus admitted initialization, drain/seal,
snapshot failure atomicity, stale freeze tickets, operation-token reuse,
version/nonce/root-ID/epoch bounds, and poison retention. Sanitizers and the
actual registry bridge tests add software evidence only, not VMX validation.

## Reusable opt-in registry bridge

`typed_directory_registry.{h,c}` implements the real registry transaction
callbacks. A provider reserves stable slot plus receipt storage before the
registry allocates metadata/root pages. The provider also supplies retirement
flush evidence. The bridge never derives a full history mask from the local
flush already performed by the sparse destructor. It keeps failed initial
roots (generation zero), callback failures, and failed retirement receipts
owned and traceable. Slots and receipts are not recycled by this first bridge.

After actual PIN/UNPIN/CLEAR_PINS operations, their `end` callback reconciles
the registry's counts into directory records before ending the admitted
operation. This occurs within the external serialization boundary. Because
legacy transaction `end` is void, a reconciliation failure may occur after a
registry operation has produced its normal return value. **Every entry or
publication caller must also check `bridge.faulted` and directory poison.**
A true registry return alone is not permission to enter. The test forces
version overflow during pin reconciliation: the actual VMCS-protecting pin
remains, bridge/ directory become poisoned, later operations are refused, and
resources are retained.

No direct sparse refill is covered automatically by registry callbacks.
Such users obtain stable identity using `td_registry_identity`, acquire a
separate directory operation and register/revoke their refill sources under
the caller's serialization. Neither this accessor nor the bridge grants a
hardware refill permit or performs typed backing validation.

If an attached-root binding cannot be reconciled, the bridge poisons and
retains the stable slot and actual item reference. An unattached retained
record is an unresolved obligation, not proof that no root exists. Sealing
and retirement reject it. A snapshot is never permitted to silently omit an
owned but unbound root. Poison does not automatically provide recovery or
permit freeing stable slot/bridge/operation storage.

Additional `/tmp` sanitizer tests in `typed_directory_registry_test.c` use
the actual context registry and sparse allocator: eviction, reset, pins and
freeze, reservation OOM, allocator OOM, failed initial flush, durable receipt
failure/retry, missing remote history evidence, and reconciliation overflow.

### Retirement versus freeze ordering

An unfinished RETIRING record prevents close without changing the gate or
epoch, so a missing flush receipt can be retried in the original epoch. Once
close wins, even an already admitted operation cannot begin a new retirement.
Seal also rejects RETIRING defensively. Thus a retirement ticket cannot be
stranded by a freeze/reopen type-epoch change. Complete its receipt and forget
the record, or explicitly retain it and later obtain a fresh retirement ticket;
never reuse old-epoch evidence. Tests cover both close/retirement orderings and
real registry receipt failure followed by successful retry and epoch advance.

## N38A final verification (2026-11-09)

The independent review reproduced incomplete retirement crossing an epoch
boundary as permanent retention. Closing now rejects unfinished RETIRING
without side effects; a closed directory rejects new retirement; seal also
rejects RETIRING. Real registry retry succeeds in the original epoch before
later epoch advancement. Eight independent pure sanitizer suites passed.
Full cloud regression `efi/build/nested38a-review-full.log` completed with
74 marked tests plus console-memory, `FINAL_STATUS=0`.

Shared registry/sparse changes relinked old profiles. All three following
new binaries passed actual isolated archbook KVM two CPU 256/512 MiB tests
(six runs, correct profile PASS, no errors or remaining QEMU):
- owner eviction: `e7f41592c29602b6e2fd9c46374b7227a6e2dafe41403855c50b309d05eaccd2`
- typed UC: `a8e29eb4786643e46f004d6ccc01b52ac79a70fa6a0d8e70d7f3138641de80d5`
- typed WT: `24053e92d5231af5e86714c3d34f01f302de24efd708b91ceca9dec9a7183e39`
Evidence: `libfile_1098a653ba248191b80d95431e266cfc`.
This is non-opt-in hardware regression, not hardware exercise of the new
transactional dynamic-directory path. That remains a separate milestone.

## N38B hardware candidate: fixed membership, two contexts per owner

The opt-in `VMX_NESTED_OWNER_TYPED_CONTEXT_TEST` profile integrates the real
registry bridge with two owner-private registries/pools. Both contexts are
created through reserve/attach/publish before first guest entry. Membership
then stays fixed: no runtime creation, eviction, migration, A/D or typed PAT0
support is claimed. EPT12 roots 0x1001e/0x1401e are guest-read-only WB structures.
Both map f000 to one dedicated data backing; an unrelated WB witness at 30000
maps different WB pages to distinguish actual EPTP selection.

Nine roots are audited: two EPT01, four actual sparse EPT02, two unexecuted
spare roots, and the preserved fixture root. Stable directory identities are
bound once to DTL slots. Subsequent snapshots resolve by complete identity,
validate exact membership, physical EPTP, cache generation, pins and refill
counts; they do not reinitialize live DTL tokens. Initial hardware history is
zero. Every authorized physical entry records its actual 01 or selected 02
root under a short metadata lock. `dtl_root_residency_extend` only grows the
stopped-consumer ledger history from the sealed directory; it cannot alter
root identity or shrink historical consumers.

Every registry callback separately takes the metadata lock. Allocation and
physical INVEPT run outside it, covered by the admitted operation. Each lazy
fill also has a whole-call operation permit. Consumer checkpoint gates close
before resetting **all four** private caches. Reset callbacks clear their
refill sources; the coordinator revokes both virtual-table sources and only
then closes/drains/seals the directory. No select, pin or refill can start
while sealed. The full MP break-before-make protocol is retained. Four stable
tokens and owner stamps are prepared before directory reopening, DTL opening
and finally consumer opening. Epoch 2 is initial WB authorization, 3 UC, 4 WB.
The baseline epoch increment itself is not a physical type transition.

The trusted L1 executes six exits per CPU: A/B WB warm, A/B UC read/write,
A/B restored-WB verification, with native VMWRITE EPTP switches. Distinct words
avoid concurrent data writes. All four contexts survive resets with their
root allocation identities unchanged. Cleanup uses actual VMCLEAR, guarded
local destruction receipts, local owner retirement flush, VMXOFF and AP park;
only restored WB and zero live tokens permit backing/pool release.

Build: `make smp-nested-typed-contexts`. Short checks: `tests/nested38b.sh`.
TCG without VMX is a capability skip, not proof of this hardware protocol.
The candidate requires independent safety review and isolated two-CPU KVM.

N38B pre-hardware review rejected the first `4d1fb45c` candidate: the actual
owner start path required an obsolete, unassigned `contexts_prepare` callback.
The TCG capability skip did not execute that path. This candidate is not a
hardware result. The corrected bootstrap must be tested through the same
helper called by actual owner start, with real registry/pool/typed binding and
an injected flush backend, including missing required wiring failures before
allocation. No empty callback is used to hide the obsolete requirement.

The corrected candidate is kernel
`8f6eae22912d4785a3fcab322ac90b4eb8c24b85a462443ac57f463324d6e90a`.
The obsolete callback was removed. `nested_owner_sparse_start` calls
`nested_owner_sparse_bootstrap_contexts`; its test links actual sparse,
registry, pool and typed code, verifies two-context assembly and reset/stop,
and rejects missing wiring before allocation. OOM, initial flush and ready
callback failures retain/clean up through the actual registry. Forced target
rebuild and `nested38b.sh` passed (`/tmp/n38b-fixed-build.log` and
`/tmp/n38b-fixed-short.log`); TCG remains a capability skip. Independent final
review and KVM are still required.

## N38B hardware evidence (2026-11-09)

Initial `4d1fb45c...` candidate was refused at review before hardware: a
required contexts_prepare callback had no assignment. The residual callback
was removed; actual start now uses a shared executable bootstrap helper,
tested against real registry/sparse/pool/typed components for successful
two-context assembly and missing setup/OOM/flush failure retention.
Corrected kernel `8f6eae22912d4785a3fcab322ac90b4eb8c24b85a462443ac57f463324d6e90a`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs contain `SMP.N38B_PASS owner_contexts=4 no_eviction=1
all_roots_audited=9 stable_tokens=4 WB_UC_WB=1 restored_before_free=1`,
without errors or remaining QEMU. Evidence:
`libfile_c3e138f5159c8191a18b5c77f934359a`.
This exercises the opt-in directory/transaction bridge on hardware with two
prebuilt contexts per owner. Membership changes and eviction remain excluded.

N38B final independent full regression: `efi/build/nested38b-review-full.log`,
75 marked tests plus console-memory, `FINAL_STATUS=0`; corrected kernel and
EFI hashes unchanged from hardware evidence above.

## N38C-A pure dynamic typed root lifetime

The opt-in `typed_root_lease` and `typed_root_registry` modules separate UC
cache-root retirement from the data backing's WB-only final release. Stable
full-identity tokens survive snapshot reordering, while durable retirement
receipts replace scans of freed roots. The bridge drives the real registry
and private owner pool in isolated tests; it is not connected to runtime.
See `TYPED_ROOT_LEASE.md` for proof obligations and the conservative close
contract. `efi/tests/nested38ca.sh` passed the old eight suites and both new
sanitizer suites; independent review remains pending. N38B images are unchanged.


## N39A root-local commit provenance

The shared `td_retire_pending` helper validates an admitted operation and the
complete RETIRING record/ticket without requiring an unchanged directory
version. `td_receipt_matches` validates linked durable receipt storage and all
root/ticket fields, including retirement epoch/nonce and commit version. Both
are serialized bookkeeping helpers, not hardware invalidation proof.

A record stores its retirement epoch independently of the directory's current
epoch. `td_forget` checks the exact receipt, including cache generation and
retirement nonce, even if forget is delayed across a later type transition.
Generic directory retirement may still span different admitted operations;
only a particular registry provider callback attempt binds its operation nonce.
The new opt-in typed-root provider uses these root-local checks while the old
provider/runtime remain serial. See `TYPED_ROOT_LEASE.md` for pure schedules,
limits and the unchanged freeze/backing-release obligations.
