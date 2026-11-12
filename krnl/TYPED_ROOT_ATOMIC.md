# N41B: atomic source/private-cache lifecycle bridge

This is the production software bridge between N41A's registered immutable
atomic source catalog and the real owner-private sparse contexts, typed root
lease/directory and root-scoped local retirement provider. It is not another
cache simulator, but it is still host-tested software, not a new VMX profile.
No new kernel, ESP, KVM, physical INVEPT or backing-free authority is claimed.
Existing N35 runtime and N40 AD0 runtime profiles are not rewired.

## Separate immutable profile

`trl_init_atomic_wb`, `typed_root_registry_init_atomic_wb` and
`tr_atomic_init` opt into WB/IPAT1/AD1. Both the exact virtual key and complete
physical EPTP must end in `0x5e`, and the actual private sparse tree has A/D
mode enabled. Catalog and typed directory domain/incarnation must match; this
bridge grants no cross-domain source access. The old lease/registry constructors retain their AD0 behavior
and reject the atomic lease. The old public fill, lookup, miss and source-miss
helpers reject an atomic bridge before any source callback. Direct sparse
publication is also gated by the active atomic operation. UC, WT, PAT0 and
WB-to-UC type transitions are not added to this profile.

The existing positional context transaction ABI is unchanged. A separate,
fresh-registry-only guard extension checks warm selection before modifying
`active`, LRU or tick, and runs pre-reset/pre-retire work before sparse reset,
flush or destruction. Existing void lifecycle callback ordering is unchanged.
Only the new atomic profile requires zero VMCS pins even for reset; callers
must clear the physical VMCS reference first. Old unguarded reset behavior is
unchanged. Consumer-stopped and current-CPU hooks remain trusted runtime
attestations, not facts established by these host tests.

Each nonrecycled stable root slot has an immutable creation-time `nrc_root`,
a persistent source session, and a separately saved source nonce. Warm selection
matches the entire original binding and current catalog membership; replacing
the supplied binding or merely finding the same numeric EPTP is insufficient.
A terminally closed catalog rejects warm selection/new faults. The persistent
source session is intentionally conservative and survives cache resets. Its
presence is not an active updater and does not make catalog idle/drain a usable
A/D maintenance predicate.

## Whole fault operation and durable traces

One stable owner-owned `tr_atomic_fault` holds the admitted directory operation
and nonce, transient refill registration, slot identity, typed token/type epoch,
source/cache snapshots, NAC source/updater permit and independent NAC ticket.
The transient registration is acquired under the short shared metadata lock;
NAC admission and every source callback run outside it. The cache guard checks
the physical CPU, stopped consumer, active item, stable slot, complete physical
EPTP, allocation lifetime, cache generation, virtual source key and current
saved typed identity. The directory lock never spans allocation, atomic table
access, source callbacks or invalidation.

A new complete atomic trace is registered as a persistent directory refill
before `nested_ept_sparse_fill_tracked`. A read with writable effective mapping
always installs an actual RO private leaf, even when a peer has already set the
shared source D bit. A cached write needs a registered usable trace and a tracked
leaf. The full `nac_trace_guard` runs before any new CAS, then a fresh atomic walk
must agree with the complete cached mapping before actual sparse promotion.
Source, updater and transient directory permits remain held through sparse
publication/promotion, its flush callback and final `nac_complete`.

A failed final guard is fatal even if all cleanup succeeds. Cleanup releases
NAC first, then the transient refill, then the directory operation. Each flag,
nonce and owning pointer stays intact until that individual cleanup succeeds.
The bridge never substitutes the old stack-based miss helper. A failed NAC
cleanup can be retried with the saved tickets after the real interrupted lock
owner finishes. A poisoned directory does not magically allow refill drops:
if they cannot complete, the stable registration, operation, root and backing
remain retained. Poison is not guest-entry or reclamation authority.

Trace storage is caller-preallocated and deliberately not recycled in this
increment, including after reset. Stable root/receipt slots are also finite and
never recycled. These are explicit bounded lifetime-throughput limits, not
unbounded allocator fallbacks or arbitrary-workload progress promises.

## Reset and retirement

Pre-reset/pre-retire checks require the actual local consumer stopped, zero
VMCS pins and no active fault. Each owned trace is marked unusable, then its
persistent refill registration is individually dropped under the admitted
transaction. A failure keeps that trace's registration and all unprocessed
storage intact. Outstanding foreign/unrevoked refills still block progress;
`td_retire_begin` has not been weakened to tolerate them.

Reset then uses the real sparse reset and flush. It changes only the private
cache generation; source structural generation, A/D epoch and type epoch do
not advance. The trace records and persistent source pin remain stable.

Retirement performs sparse invalidation while all storage is owned, then the
root-scoped production provider independently validates and invalidates the
complete physical EPTP. Its pre/post checks also match the retained source
binding and independently saved pin nonce without a source callback under the
directory lock. Pre-revocation checks validate the actual pin unlocked; a
terminally closed catalog still allows those existing pins to retire safely. A durable `td_receipt` is committed and its exact commit
version is consumed by `trl_retire`. The new fallible, unlocked pre-forget hook
then releases the persistent source pin. Only after it succeeds may the bridge
forget the record and sparse destruction return pages to the private pool.
A source release failure after receipt leaves a RETIRED record, receipt, typed
retirement proof, source pin and tree represented in stable storage; the item
has not been forgotten or freed. This is not a nonfailing post-free callback.
Retired receipts outlive the old tree and cannot validate a new allocation
that happens to reuse its HPA.

The local provider continues to reject remote historical CPU bits. Root-local
receipts tolerate unrelated peer directory changes during unlocked provider
work; they do not provide distributed invalidation or source backing release. The
legacy `trl_backing_release_ready` query explicitly rejects this atomic profile
because its evidence does not cover catalog/updater lifetime reclamation.

## Validation

`efi/tests/nested41b.sh` compiles the actual bridge, contexts, sparse trees,
private pools, catalog, walker, updater, lease, directory and local provider
with O2, Werror, ASan and UBSan. The source authority and invalidation hooks are
controlled test implementations. The tests cover shared-lower-path A/B and
distinct C, two owner caches with budget two and A/B/C/A eviction, first writes
in each private cache despite peer D, persistent-trace retirement blocking,
receipt/source-release-before-root-free, reset generation separation and
warm-selection binding corruption without LRU/active mutation.

Failure checks cover every candidate and sparse-fill allocation position,
source/updater/directory acquisition, persistent registration, source authority,
fill/promotion flush, final guard, NAC updater/source cleanup, pre-reset flush,
provider evidence, durable commit, persistent-source cleanup and a separate
directory-end cleanup failure after successful transient drop. Trace-budget
exhaustion also fails closed without recycling old trace storage. Retained
state is checked explicitly, not mistaken for leak-free successful teardown.
The sanitizer runner disables LeakSanitizer because intentionally poisoned
retention is part of the protocol and the execution container restricts ptrace;
ASan and UBSan remain enabled. Independent review and any final freeze/full
regression are recorded separately. No physical consumer or INVEPT claim
follows from mocked hooks.


## Independent focused review

The independent production suite and nineteen weakening mutants passed with
O2/Werror/ASan/UBSan, followed by separate GCC TSan runs. It includes seventeen
warm-binding corruptions before selection mutation, twenty-three cached trace,
mapping and structural corruptions rejected before write CAS, all fifty-five
local consumer-guard positions including the post-publication final guard,
both NAC cleanup-failure orders, source-retirement/provider failure retention,
terminal-source-close cleanup and the explicit profile/domain/backing-release
exclusions. Actual root-HPA reuse at generation one rejects an old trace and
its invalid recorded pointer before any source-pointer traversal.

Twelve actual dual-thread owner-private churn schedules commit 432 durable
receipts and complete 864 whole-fault operations. A separate barrier-controlled
real-thread witness mutates the peer's directory inventory between the local
provider's bound invalidation and retirement commit. Every successful root
free is checked against the production local provider and typed durable
receipt, plus completed persistent-source release. These are actual software
threads and real production private pools, but CPU identity and invalidation
remain test hooks. No hardware CAS collision or physical INVEPT is inferred.

The top-level N41B runner includes this independent suite. It retains evidence
only in a fresh directory selected by `N41B_REVIEW_EVIDENCE`; ordinary runs use
and remove temporary evidence storage. The original independent v1 and final
v2 records remain separate. Final v2 verified 91 unchanged production/header
hashes, with result SHA256
`fb89b15033b813aa0252b7f5d0ad24229687b254a5a64437bec401afc52c1493`.
Source-only freeze and the later complete regression remain distinct from
this focused acceptance. B introduces no ESP/kernel or VMX runtime claim.


## N41B final verification (2026-11-10)

A fresh full cloud regression accepted all 85 unique top-level suites: N41B
followed by the exact preceding N41A 84-suite inventory, including console-memory.
No old acceptance was reused. The independent final audit verified 170 ordered
test/acceptance markers, 85 durable fresh ledger records and 171 before/after/
final hash guards, with no missing or duplicate suites and `FINAL_STATUS=0`.

All 485 non-build source hashes matched before and after every suite. The
source-only frozen manifest remains
`67ea2da0dd2fdd827524aaa2a34003bd2d69169f2c90da7914a1b3eec866a7b9`;
its original archive remains
`a6d2bbad6cba287d01a05959f7d39f11003c05fa69b8650656b1bca6f3ce76c3`.
All 2,501 protected frozen/prior/evidence files matched, including the
original 1,903-file inventory, the latest N41A full-regression evidence and
preserved pre-run legacy working ELF/ESP artifacts. The 539-entry frozen
package and separate independent baseline were also rechecked. Reports are
`efi/build/nested41b-final-regression.log`, `nested41b-final-accepted.jsonl`,
`nested41b-final-guards.jsonl`, `nested41b-final-regression-result.json`,
`nested41b-final-independent-verification.json` and
`nested41b-final-regression-report.txt`.

The fresh N41B run included the actual production bridge, contexts, pools,
source catalog and owner-local provider under O2/Werror/ASan/UBSan, nineteen
rejected weakening mutants and separate GCC TSan runs. Twelve dual-thread
churn schedules covered 432 durable receipts and 864 whole-fault operations;
a separate controlled peer interleaving checked root-local evidence across
unrelated directory progress. CPU and invalidation hooks remain mocks.

This is host-only lifecycle/refill acceptance. No new N41B kernel or ESP,
VMX/KVM result, physical INVEPT, hardware A/D-clear/retrack round, cross-domain
source access, type transition or backing-release authority is claimed.
Legacy mutable working outputs were rebuilt during regression. Those bytes
are not fresh hardware evidence for earlier profiles; their original frozen
kernels and evidence remain unchanged. Trace/receipt storage remains finite,
and fatal cleanup failures conservatively retain resources.

This is a documentation-only append after all tests and the independent audit.
The original frozen copies, source manifest and archive are unchanged. This
document's before/after hashes are recorded separately in
`nested41b-final-documentation-update.json`; all other 484 source files and
all protected evidence remained unchanged. No production code, test or build
recipe was edited after freeze.
