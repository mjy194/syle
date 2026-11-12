# N41A: atomic source catalog and whole-operation permits

This is a pure reusable substrate, not a new VMX runtime profile. Existing
N35 single-root atomic runtime, N40 AD0 catalog runtime, typed UC/WT bridges,
and frozen hardware artifacts remain separate and unchanged in scope.

## Explicit source authority

`nrc_init_atomic_wb` creates a separate AD1-only catalog. Roots match complete
EPTPs with low bits `0x5e`; AD0 registrations are rejected. Every registered
span is WB/IPAT1. Tables have guest permission R only, with independently
supplied pinned writable L0 authority. Table registration requires an explicit
writable view; each authority resolution rechecks object identity, allocation
lifetime, HPA, host view, permissions, type, IPAT and writable pin attestation.
The ordinary AD0 registration, resolver and walker APIs reject this mode before
accessing any table word. Existing AD0 catalogs retain their old behavior.

The trusted authority must prove actual owned WB memory, entire pinned host
pages, structurally immutable entries and all guest aliases read-only. Every
reader and writer of these table words, including diagnostics and any other
catalog, must use aligned lock-free 64-bit atomics. A second AD0 catalog with
ordinary reads over the same storage is forbidden. This global alias proof is
not inferred from a callback, pointer, local mode bit or returned Boolean.
Neither catalog constructor performs a physical alias audit or pins memory.
There is no online structural replacement, mixed AD0/AD1 source access, cache
type transition, guest structural writer or backing-release authority.

`nested_ept_atomic_walk_wb_ipat` is a separate opt-in entry point. It requires
WB/IPAT1 leaves, returns explicit WB walk/leaf type fields, and keeps the old
atomic walker IPAT0-only and non-explicit. Both require AD1, four levels and
4 KiB leaves. Permission-zero entries terminate without interpreting ignored
payload; reserved/unsupported present formats remain unsupported. Complete-path
L0 backing failure takes precedence over L1 permission denial. The new API
labels backing failures `L0_BACKING_DENIED`, never architectural reason 49.
CAS uses the currently loaded entry OR only this operation's A/D bits, never
replays remembered D, and preserves the existing partial-effect status rules.

## Whole-operation permit

`nac_begin` composes a catalog source session and an actual updater token.
The gate is scoped to catalog domain/incarnation/identity/lifetime; its AD epoch
is independent of every source structural generation. The caller supplies a
cache identity containing owner, complete physical EPTP, root HPA, allocation
lifetime and cache generation. A trusted unlocked local guard must verify that
identity against the actual owned private root, stopped physical consumer and
external refill permit. The trusted guard context must also bind the actual
cache virtual key to `permit.source.root.full_eptp` under that same admitted
external operation. NAC does not register this pairing in a directory. Passing
the same identity struct back is not that proof.

The source and updater permits remain held after `nac_walk`, including on
failure, through the caller's tracked sparse fill or cached-write promotion,
local INVEPT and final guard. `nac_complete` performs the final full guard and
cleanup. Its false result is fail-closed even when cleanup succeeded; the caller
must retain any possibly installed cache tree. `nac_end` is cleanup alone and
makes no statement about publication or guest-entry safety. Metadata locks
never span an authority/cache callback, table access, CAS or local flush.

Stable caller-owned permit storage and independently saved nonce tickets must
survive failed acquisition or cleanup. A failed begin can retain an acquired
source session; the output ticket is saved once ownership starts even if the
function returns false. Cleanup releases the updater before its source pin.
Failed cleanup poisons both admissions and retains every unreleased resource.
Cleanup of a poisoned permit may be retried with the original ticket when the
interrupted lock holder really finishes; no lock stealing occurs.

## Cached traces and lifetime limits

A `nac_trace` binds the complete source root identity, including catalog and
source lifetimes, owner, structural generation and exact virtual EPTP; complete
private cache identity; target GPA; independent AD stamp; atomic path and final
mapping. Reusing a trace requires a fresh current permit. The producer's old
nonce never supplies durable authority. `nac_trace_guard` first matches every
durable identity and expected GPA, then re-resolves table HPA/view and atomically
checks structure and final backing. It makes no A/D changes and rejects stale
identity before following any recorded source pointer. A cached write must pass
this guard before starting its new atomic walk/CAS and promotion.

Trace storage does not itself retain source backing or grant refill rights.
Before traces escape a permit into a real cache, the runtime must register and
pin their source/cache lifetimes, gate every external refill and revoke those
traces before any root retirement receipt or free. N41A does not provide that
registry bridge and does not relax typed_root_registry, typed_root_lease or
local-provider AD0 restrictions. No durable retirement receipt is newly claimed.

Terminal catalog close remains permanent. Reusable A/D maintenance uses the
separate updater gate only after stopping every actual consumer, draining all
operations, revoking traces and successfully invalidating every owner cache.
Catalog idle/drain alone never authorizes clear, reopen, type change or release.
Structural generation, AD epoch, cache generation, allocation lifetime and type
epoch are distinct. All-owner invalidation attestations in host tests are mocks.

## Focused verification

Run `efi/tests/nested41a.sh` for O2/Werror/ASan/UBSan production tests. It creates
only temporary executables. Tests include two exact EPTPs sharing a lower path,
a distinct third path, full authority-field rejection, old API exclusion, full
trace identity and same-storage nonce ABA, all cache-guard failure seams with
partial CAS effects, terminal close/drain, failed begin/end retention, and twenty
real dual-thread admissions followed by a stopped test-only epoch transition
and A-only retracking. Real sparse AD1 trees are filled read-only despite peer D;
write promotion executes the mock flush while both source and updater permits
remain held. Failed flush retains and poisons the tree. Allocation/free counters
verify successful cleanup; LeakSanitizer is unavailable under the execution
container's ptrace constraints, while AddressSanitizer/UBSan remain enabled.

The top-level suite also runs independent production walker/catalog checks with
ASan/UBSan and separate GCC TSan. They include fifty actual two-thread schedules
with barrier-forced CAS conflicts, 2,400 six-thread source/walk/trace operations,
an independent real sparse RO/flush-retention witness, full durable identity
and atomic authority corruptions, and twenty-six rejected weakening mutants.
The independent runner uses temporary evidence storage by default; explicit
`N41A_REVIEW_EVIDENCE` selects a new retained output directory. Later tests do
not overwrite the original review evidence. Old focused N35A/N40A, N40B pure
runtime adapter/gates/completion and N35 owner-ad tests also pass. No full regression,
new ESP freeze, KVM result, physical INVEPT, simultaneous hardware CAS collision
or multi-root runtime completion is implied by these host tests.

## Next separate increments

1. Add a WB-only AD1 private-cache lifecycle/refill contract with registered
   persistent traces, explicit revocation, root allocation lifetime and durable
   owner-local retirement receipt. Do not weaken UC/WT or AD0 typed contracts.
2. Wire the controlled multi-root catalog and whole-operation atomic permits
   into two fixed owner CPUs with actual private EPT02.AD=1. Keep guest CR3
   paging pages separate from immutable EPT12 tables; paging-structure accesses
   under AD1 use write semantics.
3. Prove shared and distinct source paths, first-write traps despite peer D,
   root switches/eviction with trace revocation and receipt, then an all-owner
   invalidate/clear/reopen round with read A1D0 and fresh write D1. Freeze exact
   source and ESP before independent hardware acceptance and full regression.


## N41A final verification (2026-11-09)

A fresh full cloud regression accepted all 84 unique top-level suites: N41A
followed by the exact preceding N40 83-suite inventory, including console-memory.
No old acceptance was reused. The ordered test/acceptance markers, durable ledger
and 169 before/after/final hash guards matched exactly, with no missing or
duplicate suites, and the runner ended `FINAL_STATUS=0`.

All 478 non-build source hashes matched before and after every suite. The frozen
source manifest remains
`776d438ad2672c619577ea1a62746b4b056e6a462aecc9be19685b7d4b0c1c99`;
its original archive remains
`17aa28114d79043d161d9ebaa04bb93f2357a4bb71c01af32ec06d8c06f7b17c`.
All 1,774 protected frozen/prior/evidence files matched, including the
original 1,042 protected inventory, latest N40 final evidence, and preserved
pre-run working ELF/ESP artifacts. The independent final audit verified all
168 ordered markers, 84 fresh ledger entries, 169 guard records and all hashes.
Reports are `efi/build/nested41a-final-regression.log`,
`nested41a-final-accepted.jsonl`, `nested41a-final-guards.jsonl`,
`nested41a-final-regression-result.json`,
`nested41a-final-independent-verification.json` and
`nested41a-final-regression-report.txt`.

Independent focused production checks passed O2/Werror/ASan/UBSan and separate
GCC TSan runs, rejecting 26 weakening mutants. Fifty barrier-forced real CAS
collisions and 2,400 six-thread source/walk/trace operations were checked.
Actual sparse read fills remained physically RO despite peer D; both source and
updater permits remained live in fill/promotion flush callbacks. Failed promotion
and destroy retained owned pages. Full source/cache/AD/GPA identities, stale
nonce/epoch cases, partial effects, terminal source close, transient cache-guard
failure and both cleanup-failure orders were independently exercised.

This remains a pure reusable source substrate. There is no new N41A ESP/kernel,
VMX or KVM result, hardware INVEPT proof, typed AD1 bridge, durable trace/refill
registration, multi-root runtime reset/retrack, or new backing-free authority.
Legacy working outputs were rebuilt during regression; those mutable artifacts
are not the earlier hardware-accepted kernels. All original frozen kernels and
their evidence remain unchanged.

This is a documentation-only append after all test and independent-audit checks.
The original frozen copies and manifests are unchanged. This document's before
and after hashes are recorded separately in
`nested41a-final-documentation-update.json`; all other 477 source files, frozen
packages and prior evidence remained unchanged. No production code, test or
build recipe was edited after freeze.

## N41B separate production lifecycle bridge

`TYPED_ROOT_ATOMIC.md` describes the new opt-in WB/IPAT1/AD1 typed-context
bridge, persistent trace registrations, stable fault permits, pre-reset and
pre-retirement revocation, and durable owner-local receipt/source-pin cleanup
ordering. N41A's frozen source and evidence remain unchanged. This subsequent
host-tested integration does not enable a new VMX runtime profile or establish
physical invalidation, multi-root hardware maintenance or backing-release
permission.
