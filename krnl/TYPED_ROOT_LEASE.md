# N38C-A: dynamic typed root lifetime, pure protocol

This increment is an opt-in software protocol and real registry integration
test. It does not wire a new hardware profile, run VMX, or relax N38B's fixed
membership restriction. Existing UC/WT services and their hardware evidence
remain separate.

## Two different lifetimes

A private EPT cache tree may retire while its data backing is UC. This requires
removal of every VMCS and software refill reference, successful invalidation on
every historical consumer, and durable retirement evidence before any tree
node is returned to its pool. That operation does **not** restore the target's
host cache type and does not authorize releasing the data page. Backing release
additionally requires restored WB, completed global maintenance, all consumers
stopped, and no remaining live root references.

A typed root token names the full directory identity and its unique nonce and
type epoch. It is not a position in a scan array. Root allocation lifetime,
cache generation, directory version and type epoch are distinct. Reusing a
physical root address or selecting the same virtual EPTP does not revive an old
token or let an old receipt satisfy the new root's obligations.

## Bounded, stable storage

The caller provides stable binding/receipt storage and an explicit budget.
Reservation precedes registry allocation. Candidate construction, initialization
flush failures and failed eviction remain represented until safe cleanup.
Retirement receipts outlive tree storage. Budget exhaustion refuses a new
reservation rather than borrowing unrestricted memory or overwriting evidence.
The initial implementation need not garbage collect receipts during a domain's
lifetime; finite lifetime throughput is an explicit resource limit.

## Freeze and external proof obligations

All APIs are serialized bookkeeping, not a lock or hardware implementation.
The caller must serialize metadata operations and keep storage alive. A held
directory operation covers allocation, flush and refill work outside that
metadata critical section. The initial typed close is deliberately conservative: under the same metadata
serialization, it rejects active operations and non-transitionable retained
candidates before changing the gate. After admitted work has completed, its
zero-operation check and directory close exclude a racing new begin. It does
not implement an asynchronous wait/drain scheduler or guarantee progress under
continuous new requests. Unresolved retirement also rejects close unchanged. A snapshot distinguishes
live roots requiring a scan from receipt-only retired roots. The latter must
never be traversed through their old physical address.

Type transition still requires actual owner quiescence, all relevant EPT/TLB
invalidations, host break-before-make and cache maintenance, plus current-type
alias auditing. A pure transition attestation records those obligations; it
cannot perform or prove them. New tokens are prepared while entry is closed,
and consumers must remain stopped until all owner metadata and the new epoch
are ready. A local L1 INVEPT is not a domain backing-release authorization.

## Validation status

Implementation and the combined isolated O2/ASan/UBSan/Werror runner passed.
`efi/tests/nested38ca.sh` runs the eight existing N38A suites plus the new
typed-root policy and actual registry/pool integration suites. The parent log
is `/tmp/nested38ca-parent.log`. Independent review is pending. No N38C
hardware integration or KVM result is claimed.

The opt-in registry bridge executes real `nested_ept_contexts` allocation,
pinning, reset and guarded destruction against the supplied pool. Test flush
callbacks are mocks. Provider receipt lookup must consume already completed
hardware evidence; a future runtime must not wait for remote CPUs while
holding its metadata lock. The bridge preserves complete retirement tickets and exact durable commit
versions. N39A extends the shared directory receipt to retain type epoch,
retirement nonce and commit version as well.

The fill/lookup wrapper holds a directory operation throughout the sparse
operation. Its cache-type admission is not an HPA ownership resolver: the
caller still supplies immutable, owned, appropriately writable backing and
keeps that backing alive. This stage has no persistent external refill plan;
installed leaf metadata belongs to the tree. Adding a plan outside the tree
would require a durable refill registration until that plan is revoked.

## N38C-B runtime candidate (hardware validation pending)

The opt-in `smp-nested-typed-dynamic` target integrates the real registry with
one shared directory, private 96-page owner pools, resident budget two and
three immutable virtual EPT roots. Each owner executes A/B/C/A in WB,
C/B/A in UC, and B/A after restoring WB. The controlled serial owner rounds
are not a claim of concurrent membership mutation. Each round starts or ends
at actual physical-root checkpoints. Virtual instruction dispatch remains
opcode-driven; the expected labels are diagnostic checks only.

Stable typed slots survive cache-root retirement. Six creations and four
evictions per owner are expected, with sixteen lifetime receipt slots per
owner. Exhaustion rejects safely; 96 pages is a budget, not a progress
promise for arbitrary workloads. A new context is pinned before actual
VMCS EPTP publication; old pins are released only afterward. Guarded destroy
flushes locally and commits the full historical receipt before retiring the
typed token, forgetting the directory record and returning pool pages.
UC cache-root retirement never authorizes data backing reclamation.

The complete miss operation now holds a directory permit across lookup,
trusted EPT12 resolution, type admission and sparse fill. Metadata callbacks
use the same short lock; allocation, hardware flush and waits do not hold it.
The immutable backing contract remains required. Failed operations latch a
fatal condition and retain resources rather than attempting unsafe rollback.

Both transitions reset all live owner trees and revoke three external refill
sources before sealing a directory version. The snapshot includes five fixed
roots, live private roots and receipt-only entries. Only live entries are
scanned; retired HPAs are never dereferenced. Per-root, per-phase, per-CPU
flush evidence binds the complete identity, EPTP and freeze ticket. Fresh
host and EPT audits validate the current UC type on the return path, not the
initial WB raw snapshot. All token epochs and refill sources are prepared
before the consumer gate opens. The final WB-only receipt snapshot follows
both owners' actual CLEAR, local flush, VMXOFF and AP park.

This candidate is limited to WB/UC, IPAT1, AD0, one dedicated data backing,
fixed owner CPUs and controlled root checkpoints. It does not modify PAT or
MTRRs, remove the old single-CPU service gate, or authorize arbitrary guest
page-table mutations. Short pure tests and TCG capability skip do not prove
hardware execution; isolated KVM and independent review remain required.

The completion regression `efi/tests/nested_owner_completion.py` compiles and
executes the unchanged production completion block using both actual Makefile
profiles. N38C-B retains its >=6 creations / >=4 evictions evidence and pool
bound, while N34C2 alone retains exact 4/2 counts. Both owners, rejected short
sequences, pool overflow and invalid read/release evidence are covered with
ASan/UBSan. This is host-side validation, not VMX execution.

## N38C-B hardware evidence (2026-11-09)

Recovery review found the first e96f5b75 candidate also inherited the legacy
exact 4/2 creation/eviction assertion after its own >=6/4 checks. It was
rejected before hardware. The corrected guard excludes dynamic from the
legacy bound and retains the dynamic pool bound. A compiled-profile test
executes the unchanged production completion block under both actual Makefile
profiles; restoring the old guard is a demonstrated negative witness.
Frozen corrected kernel
`1197dd8e322ea7c820b85d902fe5185ab4f65097f9f8f3ee80a93fad72cd9937`
passed archbook isolated KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs contain `SMP.N38CB_PASS owner_budget=2 virtual_roots=3
runtime_eviction=1 receipt_before_free=1 serial_owner_rounds=1 WB_UC_WB=1
restored_before_free=1`, without errors or remaining QEMU.
Evidence: `libfile_fe96e12ef0448191bb9d60ee3399f801`.
Controlled serial owner rounds and finite receipt/pool budgets remain limits;
this is not simultaneous membership mutation or arbitrary guest structural
writes. Full cloud regression is separately tracked and was interrupted.

N38C-B corrected final regression completed after recorded interruption and
resume: `efi/build/nested38cb-fixed-review-full.log`, 77 distinct marked suite
acceptances plus console-memory, `FINAL_STATUS=0`. No missing or duplicate
acceptances. All 441 source-baseline checks passed. Frozen/current ESP and
build kernel remained `1197dd8e322ea7c820b85d902fe5185ab4f65097f9f8f3ee80a93fad72cd9937`.
Provenance: `efi/build/nested38cb-final-regression-report.txt` and
`nested38cb-final-regression-result.json`. This full cloud regression is
separate from the same-hash archbook KVM evidence above.


## N39A: root-scoped retirement evidence (pure candidate)

`typed_root_registry_init_root_scoped` is a new explicit opt-in provider ABI.
The existing init, runtime provider and fixed-root teardown retain their
serial global-directory-version contract. No runtime parallel owner churn,
new hardware profile, writable EPT12 structures or A/D combination is enabled.
The previously verified N38C-B kernel is not evidence for a rebuilt candidate.

One bridge, registry, pool and tree set remain single-writer per owner.
Distinct owners may share a directory and lease using the same short metadata
lock. A directory operation remains admitted across allocation and unlocked
flush-evidence work; the lock never spans either. The provider receives a
complete root retirement ticket plus the current operation nonce. The ticket
binds domain/incarnation, root ID/incarnation, allocator lifetime, owner, full
physical EPTP/HPA, cache generation, historical CPU mask, type epoch and unique
retirement nonce. Operation nonce identifies this callback attempt, not the
root lifetime. Retrying an unchanged RETIRING root may keep the retirement
nonce but must use a fresh admitted operation.

After the callback returns, the bridge rechecks the live admitted operation,
root/item identity, zero pins/refills, complete retirement ticket, current type
epoch, poison state and exact historical ACK mask under the lock. A valid
peer create, pin/unpin, transient refill, residency update or retirement may
advance directory version without invalidating this proof. Root retirement
itself freezes new pin/refill/residency/cache-reset admission. A same-root
retain/restart changes its retirement nonce and invalidates the old evidence.
No receipt is accepted just because a global version comparison was removed.

The committed receipt now durably retains its type epoch, retirement nonce
and exact commit version. Shared receipt matching checks every identity and
root field plus the complete ACK, nonzero flush serial and linked receipt
storage. The typed lease and bridge bind to that exact commit version, never
to an assumed `evidence_version + 1` or a later directory version. The record
retains its retirement epoch so delayed forget still checks the original
receipt after a subsequent epoch transition. Receipt storage remains stable
and bounded; root address reuse cannot revive an old proof. Directory-counter
or receipt-budget exhaustion still rejects safely and may retain resources.

This does not relax freeze: typed close still requires every admitted
operation drained, no unresolved retirement, and all live roots/refill sources
accounted for. WB/UC changes remain whole-domain stopped-consumer operations.
VMCS switch order remains new pin, successful VMWRITE, then old unpin. Virtual
L1 INVEPT remains owner-local. Root retirement in UC never authorizes data
backing release, even when all root tokens are retired.

`efi/tests/nested39a.sh` runs the existing N38A/N38C-A suites and deterministic
pause/resume tests against the actual registry, bridge, private pools and
provider callbacks with O2/Werror/ASan/UBSan. The baseline witness was executed
before production changes: while A's provider is paused unlocked, B performs
valid create, pin, fill/lookup, residency and retirement; the old bridge
rejects A and retains its tree. The new ABI succeeds with the same interleaving
in WB and UC, with B's receipt committed before A's. Actual typed snapshots
then contain two durable receipts and permit no old-HPA scans. Mutant builds
restoring either the global version fence or adjacent-commit assumption must
fail that success test.

Negative schedules cover every ticket identity field, stale retirement and
operation nonces, ended/reused operations, changed root/item state, epoch and
history, missing remote ACK, failed/zero flush evidence, pins, poison, version
exhaustion, receipt budget, old receipts, and reused physical root addresses.
Every failed retirement retains tree ownership. These are deterministic
software interleavings with mocked flushes, not concurrent CPU execution or
proof of VMX invalidation. Independent review and any later runtime integration
must be recorded separately.

## N39A final verification (2026-11-09)

Independent pure and supplemental boundary tests passed, including actual
registry interleaving witnesses, same-root mutation exclusion, exact durable
receipt version, foreign receipt rejection and delayed forget. Full cloud
regression finished with 78 marked suites plus console-memory (79 acceptances),
`FINAL_STATUS=0`; 42 verified previous acceptances were reused and only the
ambiguous interrupted nested11 was rerun. All 448 source-baseline checks and
the 11-file N39A freeze remained unchanged. Reports:
`efi/build/nested39a-final-regression-report.txt` and
`nested39a-final-regression-result.json`.

Shared layout changes were force-rebuilt before testing. Four legacy runtime
profiles passed isolated archbook two-CPU 256/512 MiB KVM (eight runs):
- dynamic: `d3d652345127dea7f48a2f87632c0509b8096f863450657f651ebd4401f014f8`
- contexts: `68b93b4d8d54f59dad8493f67dc39703f89b32dad4590f5d12f3eef9a9870061`
- UC: `c49e2dd17902d3264cd295038be36f4094034cf97eeb8fc69e60d549381fbcb3`
- WT: `daa87cf3bc6dbe24fa1d8360be81ff27d1b83eb0848fdc260839adcbba624530`
Evidence `libfile_4ccf2e9b62f881918dee53ff27e79c3b`; no errors or remaining QEMU.
These are legacy serial-provider regressions. N39A root-scoped provider is
still not wired into VMX runtime; no hardware parallel-retirement claim.

## N39B1: hardware-bound root provider candidate

The new opt-in `smp-nested-typed-root-scoped` profile uses
`typed_root_registry_init_root_scoped` and the shared production
`typed_root_local_provider` logic. It retains N38C-B's serial owner rounds,
serial fixed-root teardown, private pools/registries, budget two, three immutable
EPT12 roots, dedicated backing and WB/UC, IPAT1, AD0 restrictions. This is not
parallel owner membership churn; that witness is deferred to N39B2. Legacy
profiles keep their existing provider and completion marker.

For each dynamic retirement, the provider validates the physical CPU, owned
stable slot and linked bridge/directory/typed binding, complete supplied ticket,
exact admitted operation, current type epoch, root/item identity, zero pins and
refills, owner-only history and healthy nonexhausted metadata under the short
lock. It releases the lock and performs an additional actual single-context
INVEPT of the ticket's complete physical EPTP, even when history is zero. The
runtime hook observes the physical CPU both before and after INVEPT. The earlier
guarded-destroy flush alone cannot supply this callback-bound proof.

After reacquiring the lock, the provider revalidates the same root and operation
and the successful physical CPU/EPTP/local ACK result. Only then does it publish
stable completed evidence with full scope, exact historical ACK and a unique
nonzero flush serial. Remote history is rejected, never invented from the local
flush. The bridge independently revalidates and commits its durable receipt,
using `receipt.commit_version` rather than an adjacent evidence-version guess.
Unrelated peer directory progress does not invalidate the local proof. No lock
spans INVEPT, allocation or a wait. Any runtime provider failure latches fatal
state and retains resources. The generic module can retry a prior failed flush
with a fresh admitted operation, but runtime recovery is not claimed.

Before final backing release, an additional audit matches every completed
provider record to the stable bridge evidence, typed retirement and exact
durable receipt. This audit never dereferences the forgotten registry item or
freed tree. Whole-miss permits and new-pin/VMWRITE/old-unpin order remain
unchanged. UC root retirement still cannot authorize data backing release.

`efi/tests/nested39b1.sh` runs the production provider against the real bridge,
registry and private pools with injected physical CPU/INVEPT hooks under
O2/Werror/ASan/UBSan. Coverage includes both CPUs and WB/UC, zero history,
unlocked peer create/pin/refill/residency/retirement, failed/wrong-EPTP/wrong-CPU/
wrong-ACK flushes, stale complete scope and operation, pre/post same-root
mutations, pins/refills, poison, exhaustion, lock failure, corrupt returned
bridge evidence, fresh-operation retry and final evidence corruption. The
compiled production completion slice runs with actual N39B1, N38C-B and N34C2
Makefile flags. These host tests and the TCG capability skip are not hardware
proof. New frozen-kernel isolated KVM evidence and independent review remain
required before this candidate is accepted.

N39B1 candidate host acceptance: the production-provider sanitizer suite,
all three actual completion profiles, and the short N39A integrated pure suites
plus both old-assumption mutant witnesses passed. Independent review passed
additional stale-HPA/delayed-epoch cases and four provider mutant witnesses
(global version fence, skipped zero-history flush, omitted postvalidation,
omitted CPU binding). A force rebuild reproduced kernel SHA256
`e174cf80205ef1b69944926abf25973001bfbafd408e171779fa3f39a0dc8ebd`.
TCG exited 33 on its expected VMX capability skip, not the hardware PASS marker.
Logs: `efi/build/nested39b1-short.log`, `nested39b1-force-rebuild.log`,
`nested39b1-pure-regression.log`, and `nested39b1-tcg.log`. The frozen candidate
still awaits isolated KVM and its new full regression.


## N39B1 final verification (2026-11-09)

The exact frozen kernel
`e174cf80205ef1b69944926abf25973001bfbafd408e171779fa3f39a0dc8ebd`
passed isolated archbook KVM with `-cpu host -smp 2` at both 256 and 512 MiB.
Both runs exited 33, reported `SMP.N39B1_PASS`, six bound provider receipts
per owner, seventeen final durable receipts, final type epoch three and zero
live tokens. Both logs had no errors and left no QEMU process. Hardware evidence:
`libfile_af6df155f5c08191822fff4b3bfa055d`, archive SHA256
`6781be4d3db45678dc2f0b493721ba1558a01dfa152157ba18174a9c9ad91f74`.
This validates the actual owner-local provider with controlled serial owner
rounds; it does not claim simultaneous membership mutation or B2 overlap.

A fresh full cloud regression accepted all 80 unique top-level suites:
the previous 78 marked suites, new `nested39b1`, and `console_memory`.
No earlier acceptance was reused. The ordered test/acceptance pairs and
per-suite ledger contain no missing or duplicate suites, and the runner
finished with `FINAL_STATUS=0`. All 447 non-build source files, the build/ESP
kernel copies and the frozen candidate package remained unchanged before
and after every suite. Reports: `efi/build/nested39b1-final-regression.log`,
`nested39b1-final-accepted.jsonl`, `nested39b1-final-regression-result.json`,
`nested39b1-final-provenance.json`, and `nested39b1-final-regression-report.txt`.

Independent O2/Werror/ASan/UBSan review additionally passed both owners in
WB/UC with and without residency, 23 unchanged-output/evidence failure cases,
real bridge/private-pool peer interleavings, stale-scope rejection on root-HPA
reuse, and delayed receipt audits after later type epochs. Its tests reject
four intentional provider regressions: a restored global version fence,
skipped zero-history INVEPT, missing post-INVEPT scope revalidation, and
missing hardware CPU binding. Evidence is preserved under
`efi/build/nested39b1-independent-review/`.

This verification entry is an explicitly documented, documentation-only
postfreeze edit. No production code, test, build recipe or kernel changed
after acceptance. The original frozen source manifest remains the tested
baseline; the report records this document's before/after hashes separately.

## N39B2: actual overlapping owner-private churn candidate

The separate opt-in `smp-nested-typed-overlap` target (`N39B2FLAGS`) adds
`VMX_NESTED_OWNER_OVERLAP_TEST` to the B1 profile. B1 keeps its serial owner
rounds and its existing marker. Both keep fixed CPUs 0/1, private pools and
registries, budget two, three immutable guest-read-only EPT12 trees, dedicated
data backing and WB/UC, IPAT1, AD0 restrictions. This is not a general structural
writer protocol or remote root migration.

WB epoch one holds owner zero's first eviction, while selecting C after B.
UC epoch two reverses the schedule: owner one's first UC eviction is held while
selecting B after C. The hold occurs only after the unchanged production B1
provider has completed and revalidated its additional actual local INVEPT.
Its full scope, physical CPU/EPTP/ACK, operation and retirement nonces, serial
and current directory version are copied under the metadata lock. Publication
is release/acquire, and the bounded hold itself is outside every lock. The
bridge operation continues retaining the victim and candidate until callback
return and independent bridge revalidation/receipt commit. Failure or timeout
latches fatal state and retains allocations.

The peer waits at its first safe root preentry boundary for the held record,
then completes WB A/B/C/A or UC C/B/A. It publishes only owner-local counter
snapshots, observed successful new-pin/VMWRITE/old-unpin steps, full physical
EPTPs and a validated durable receipt. The holder is released by this immutable
record, never by reading a peer's live pool, item or counters. Both final
records are matched in full to stable provider/bridge/lease receipts. Acceptance
requires held snapshot version < peer receipt commit < held receipt commit,
as well as increased peer creations, misses, evictions and completed switches.
Neither wall-clock timing nor counters alone are overlap evidence.

B2 removes the whole-round turn gate. It retains a distinct final teardown gate
inside terminal checkpoint phase three, before returning to final L1 execution,
VMCLEAR or sparse destruction. Legacy fixed-root retirement remains globally
version-fenced and is therefore still serialized. WB/UC transitions still wait
for both actual consumers at checkpoints, drain admitted operations and pending
retirements, revoke immutable refill sources, reset/seal/audit, perform both
INVEPT/cache/TLB phases, and reopen last. Shared table scans remain stopped-only.
Full WB restoration and final receipt-only audit precede backing release.

`efi/tests/nested39b2.sh` executes the production protocol, production B1 provider,
100 real pthread schedules using actual private registries/pools/bridges, and
unchanged compiled runtime gate and completion slices. Its host VMX hooks are
mocks. Wrong phases/epochs/scopes, stale nonces, duplicate publication, missing
progress, failed/incorrect hardware evidence, timeout and lock-held waits fail
closed. Mutants restoring the global version fence or whole-round turn, or
removing the terminal phase-three gate, fail the success witnesses. All four
actual completion profiles remain covered. Independent ASan/UBSan and TSan
thread runs and completion mutants were also performed. None of these tests,
or a TCG capability skip, establishes actual hardware overlap: exact frozen
ESP KVM evidence and the later full regression are separate acceptance stages.


## N39B2 final verification (2026-11-09)

The exact frozen kernel
`acc8d3ed14a79e0bbf41a7c03d03a94a6c2a217b87d01aaf8b8d09982cf17011`
passed isolated archbook KVM with `-cpu host -smp 2` at both 256 and 512 MiB.
Both runs exited 33 and reported `SMP.N39B2_PASS`. The verified causal version
chains were WB 204 < 425 < 458 and UC 588 < 635 < 715: held evidence, peer
receipt commit, held receipt commit. Peer creation/eviction/miss/switch deltas
were 3/2/37/3 in WB and 2/2/30/3 in UC. CPU, epoch and ACK scopes matched the
held and peer roots. Both runs completed with six bound receipts per owner,
seventeen final receipts, epoch three and zero live tokens, with no errors or
remaining QEMU. Hardware evidence: `libfile_21b2861198c88191bf296fade03887fe`,
archive SHA256
`916a71193fa46e21e3fe613ac88ece7090c7364b706024e1c34281419596a24a`.

A fresh full cloud regression accepted all 81 unique top-level suites: the
previous 80 suites plus N39B2, including console-memory. No old acceptance was
reused. Every ordered test/acceptance pair and persisted ledger entry matched,
with no missing or duplicate suites, and the runner ended `FINAL_STATUS=0`.
All 455 non-build source files, B2 build/ESP/frozen kernels, both frozen candidate
packages and prior B1 final evidence remained unchanged before and after every
suite. Reports: `efi/build/nested39b2-final-regression.log`,
`nested39b2-final-accepted.jsonl`, `nested39b2-final-regression-result.json`,
`nested39b2-final-provenance.json` and `nested39b2-final-regression-report.txt`.

Independent review passed actual two-thread production-provider and overlap
module schedules under ASan/UBSan and TSan, full-scope evidence corruption,
post-provider timeout retention, the unchanged runtime gates and all four actual
completion profiles. The success witnesses reject restored global version
fencing, restored whole-round turn, a removed terminal phase-three gate,
restored legacy exact-count completion and an omitted B2 overlap completion
guard. Evidence is preserved under `efi/build/nested39b2-independent-review/`.

The full run rebuilt ordinary legacy build outputs from current guarded
sources. Those outputs are not the previously hardware-accepted B1 kernel;
its original frozen e174cf80205e package and evidence remain unchanged, with old
working B1 images additionally preserved before rebuilding. The B2 hardware
claim applies only to the exact acc8d3ed14a7 frozen kernel above.

This entry is an explicitly documented, documentation-only postfreeze append.
No production code, test, build recipe or kernel changed after acceptance. The
original 455-file frozen source manifest remains the tested baseline; the final
report records this document's before/after hashes separately. B2 remains
limited to two fixed owner CPUs, private single-writer registries/pools, three
immutable EPT12 roots, finite stable receipts and WB/UC, IPAT1, AD0. It does not
claim arbitrary guest structural writes, remote migration or runtime recovery.


## N40 controlled source authority candidate

`NESTED_ROOT_CATALOG.md` documents a new immutable source catalog/owned backing
substrate and the explicit `smp-nested-root-catalog` runtime profile. It keeps
structural source lifetime distinct from this typed cache-tree lease and does
not enable A/D or arbitrary source replacement. N39B2 frozen evidence above is
unchanged; acceptance of the new kernel requires separate same-hash evidence.


## N40A/B final verification (2026-11-09)

The exact frozen `smp-nested-root-catalog` kernel
`b716d18ae7dd402651d7a7775e080e2b611bd9f58b1fcf5d99c6ad999dd5b0e1`
passed isolated archbook KVM with `-cpu host -smp 2` at both 256 and 512 MiB.
Both runs exited 33 and reported `SMP.N40B_PASS`. Each owner produced catalog
negative-probe mask `0xf` and six bound provider receipts. Final checks recorded
seventeen durable receipts, type epoch three and zero live typed tokens.
The causal version chains were WB `0xd0 < 0x1ad < 0x1ce` and UC
`0x250 < 0x27f < 0x2cf`: held evidence, peer commit, held commit. All 499
before/after hardware inventory hashes matched. Hardware evidence is
`libfile_8446c3d378508191a3b5669234ecd891`, archive SHA256
`f8e1f5e3d44bfad16c02fff88dec92d101d6df9f6cf2aaa8ca9d8f363a1aa985`.
An initial host validator matched ordinary firmware reservation output; the
narrow correction and revalidation used the same preserved logs, without a
guest rerun. That validator history remains in the evidence package.

A fresh full cloud regression accepted all 83 unique top-level suites: N40B,
N40A and the complete preceding 81-suite inventory, including console-memory.
Every ordered test/acceptance pair and durable ledger entry matched, with no
missing, duplicate or reused acceptances, and the runner ended `FINAL_STATUS=0`.
The frozen inventory contains 475 entries: 470 non-build source files plus
five preexisting generated host-test binaries under `boot/build` and
`krnl/tests/build`. All 475 hashes matched before and after every suite, as did
three current/frozen N40 kernel paths, the original frozen archives and 1,042
protected prior/frozen evidence files. The original inventory was not relabeled
or rewritten. Reports are `efi/build/nested40-final-regression.log`,
`nested40-final-accepted.jsonl`, `nested40-final-regression-result.json`,
`nested40-final-independent-verification.json` and
`nested40-final-regression-report.txt`. A preliminary detached launch ended
before any suite acceptance; its zero-acceptance records were preserved, and
the complete persistent run began again at N40B.

Independent review exercised the production catalog, actual typed registry
and private pools, the opt-in owner adapter and unchanged runtime gate slices.
Eight irregular roots and more than 63 backing pages passed the reusable API
checks. Twenty-five weakening mutants were rejected. One hundred real
eight-thread admission/close schedules passed ASan/UBSan and a separate TSan
run. Review corrected leaf type/IPAT admission, complete-path L0 fault priority,
entry guard lock ordering and an implicit L1-root/L2 self-mapping assumption.
Evidence is under `efi/build/nested40a-independent-review/` and
`efi/build/nested40b-independent-review/`, also preserved in the original freeze.

Legacy working build outputs were rebuilt from the new guarded sources; they
are not fresh hardware evidence for old B1/B2 profiles. Their original frozen
kernels and evidence remain unchanged, with pre-run working copies preserved
separately. N40 hardware acceptance applies only to the exact kernel above.
The original frozen archive remains
`10e745bb7ffe51cca04ee218853b5b9cca4b3fa0f380efbe1ffbfcaec507dde2`, and its
source manifest remains
`4bed175d6fb5de57406b1aaf8449e485f757847fb6ab2ad0f7ca64af7cc64f89`.

This is a documentation-only postfreeze append. No production code, test,
build recipe or kernel changed after acceptance; this document's before/after
hashes are recorded separately in `nested40-final-documentation-update.json`.
The hardware fixture remains three immutable roots and 63 pages, with two
fixed owner CPUs, private single-writer caches, finite stable receipts and
WB/UC, IPAT1, AD0. There is no source replacement/reopen during cache lifetime,
unrestricted guest structural writing, shared multi-root A/D, remote migration
or runtime recovery claim. Catalog idle/drain alone does not authorize cache
type changes or backing release.
