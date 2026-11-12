# N42A: production global A/D maintenance protocol

N42A is a source-only, opt-in coordinator for the existing N41B atomic bridge.
It does not change the N41C VMX profile, the N35 single-root runtime, any frozen
candidate, or the source-backing release prohibition. Physical entry, reference
removal/restoration and exact INVEPT receipts are trusted adapter callbacks in
this stage. Host tests mock those callbacks. The existing N41C kernel is also
compiled and linked for freestanding compatibility; there is no N42A runtime,
ESP or hardware acceptance claim.

## Scope and admissions

`tam_init` binds two fresh owner-private registries to one shared catalog,
updater gate, typed directory and WB lease. The owners must use the same external
metadata lock, although their callback contexts may differ. Each registry remains
single-writer. The coordinator supports up to eight resident roots per owner and
sixteen deduplicated source table pages. Capacity exhaustion fails closed.
The catalog also has at most sixteen source roots, frozen by full identity at
initialization and revalidated before clear and reopen. All registered catalog
table pages must be reachable from the published roots.
A page used at inconsistent depths, including a cycle, is rejected.

The normal context transaction predicate and its actual `td_begin` run under one
existing metadata lock. Fault and entry admissions check the same state under
that lock. Source authority callbacks, CAS, physical operations, sparse resets
and waits run outside it. An admitted whole fault may finish after software close,
including a fault paused before `nac_begin`. Updater closure is delayed until
those whole directory operations drain. Persistent catalog pins are neither
updater references nor evidence that updater work drained.

An entry that encounters an already-closed software gate before admission is
rejected without poisoning. An already-admitted entry whose final post-authority
guard races close poisons and retains, rather than authorizing execution from
an obsolete decision. The runtime adapter must separately close real dispatch
admission: the bridge's entry operation ends before actual VM entry. N42B must
use cooperative stopped checkpoints or otherwise prove that physical entry and
existing operation guards remain correctly serialized.

## Ordered round

1. Reserve a nonce ticket containing the complete old A/D stamp and close normal
   software admission under the metadata lock. `TAM_STOPPING` is not drainable.
   The trusted adapter closes physical entry and proves both consumers stopped.
   Only successful completion moves to `TAM_CLOSING`.
2. Drain admitted directory operations while their updater gate is still open.
   Then close updater admission and drain actual updater leases. Polling has a
   fixed bound. Timeout poisons instead of stealing a lock or rolling back.
3. On each owning CPU, snapshot every current root, including inactive contexts.
   Match complete source and private identities, allocation lifetime, exact EPTP,
   history and generation. Reject unfinished candidates/retirements and foreign
   history. The adapter removes all actual owner VMCS references first. Only
   then do dedicated stable maintenance operations reconcile metadata pins to
   zero and revoke every persistent trace/refill registration. The immutable
   source sessions remain pinned. Both owner revoke acknowledgments are required.
4. Enumerate each unique catalog TABLE page through the actual atomic authority.
   Snapshot every word, traverse all published roots and validate page depth and
   supported present structure before the first clear. Nonpresent payload is
   ignored and preserved. Nonleaves clear A only; leaves clear A and D. Every
   update is an acquire-load/acq_rel-CAS of the current word with those bits
   removed, checked against the immutable snapshot. CAS retries are bounded.
   Changed counters count only successful value-changing CAS operations.
   A final full authority/word audit detects late failures after the final CAS.
5. Each owner resets every snapshotted private tree. The additive guarded sparse
   reset zeros the root, executes its ordinary flush, then obtains and validates
   an explicit exact receipt before any subordinate page is freed or generation
   advanced. Receipts bind maintenance ticket, complete root identity, EPTP/HPA,
   owner CPU, historical mask and old/new cache generation. They are retained
   individually. Directory cache generations are committed only after successful
   guarded reset. The ordinary unguarded API's existing behavior is unchanged.
6. After all owner reset acknowledgments, prepare the next A/D stamp while the
   old updater gate remains closed and drained. Pin each active root before the
   trusted adapter restores its actual VMCS reference. A failed or uncertain
   restore retains that pin. No old trace storage becomes usable again.
7. Require all exact owner reset/prepare acknowledgments, unchanged source/type
   identities and a fresh complete source-clean audit. Reopen the updater at the
   next A/D epoch, open physical admission via the adapter, then open software
   admission last. The runtime's waiting-owner barrier must not release until
   `tam_finish` returns; a probe during `open_entry` is denied without poisoning.

Only the A/D epoch advances globally. Source generation/lifetime, catalog
identity, type epoch and root allocation lifetime do not change. Every surviving
private root advances its independent cache generation exactly once. Subsequent
reads create fresh traces and physically read-only private target leaves even
when a peer later sets source D. A new write must execute its own production
fault/CAS/promotion path; old D is never replayed from revoked traces.

## Failure and lifetime boundaries

Wrong-ticket, wrong-CPU, duplicate and out-of-order calls reject without changing
the round. Once an action starts in the correct phase, a callback, identity,
source-authority, flush or cleanup failure poisons admissions and retains remaining
resources. Partial clear/reset effects are not rolled back. Rejected reset receipts
retain every subordinate page because validation precedes freeing. A later
metadata failure may occur after a valid exact receipt permitted subordinate-page
release; the private root and source pins still remain retained and no reentry
is authorized. Stable maintenance operation storage survives failed cleanup.

Adapters must call `tam_abort` for owner-wait timeouts or uncertain hardware
outcomes. The protocol does not pretend that poisoning stops an already-running
consumer, that a Boolean is physical invalidation evidence, or that an atomic
load/CAS prevents an unauthorized structural writer. The trusted whole-lifetime
WB ownership/read-only-alias contract remains mandatory.

Catalog terminal close is not used for temporary maintenance. The typed directory
is not closed/reopened through its type-transition API, and no type epoch is
advanced. There is no cross-domain support, source replacement, type transition,
source backing reclamation or grant of new backing-free authority.

## Verification

Run `efi/tests/nested42a.sh` after sourcing `efi/scripts/cloud-env.sh` in the cloud
workspace. Its tests execute the production coordinator, atomic bridge, catalog,
private pools, sparse contexts and typed directory under O2/Werror/ASan/UBSan.
They first build the existing N41C kernel in a fresh temporary source tree with
the actual Makefile's freestanding O1 flags and nostdlib link. The atomic bridge
must have no unresolved libc memory helper, and the final kernel no unresolved
symbols. Restoring the old large aggregate initializer only in that temporary
tree must reproduce its undefined `memset` link failure. `tam_init` uses the
existing explicit volatile-byte initialization pattern and is also tested from
dirty storage, with every inactive field cleared before the first round.
LeakSanitizer is disabled for intentional fail-closed retained resources and
container restrictions. The independent runner adds real software-thread
admission schedules, source/receipt corruption cases and weakening mutants, including concurrent owner maintenance and entry/close
ordering.
Physical CPU identity, VMCS references and INVEPT remain explicit test mocks.

The implementation test performs two complete rounds with A/B sharing lower
source tables and C using a distinct path. It inspects actual private page-table
words after read retracking, proves target A1D0 before new production write
promotion, and checks source/type identity and private allocation reuse. It also
covers receipt identity corruption before free, partial clear, failed flush,
reference removal and uncertain restore, separate directory/updater drains,
missing owner progress, stale/wrong tickets and bounded drain timeout.

Focused host acceptance, source-only freeze, full regression and eventual exact
new-image VMX acceptance are separate stages. No earlier acceptance is reused as
proof for this protocol. N42B must wire the real maintenance-stopped proof,
reference lifecycle, all-owner invalidation and guest clear/read/write checkpoints
before any new hardware claim is made.

The legacy N41B/N41C independent runners now include the new production
maintenance include in their source hash guards. N41C's existing final-entry
revalidation mutant text target was updated for the added admission conjunct;
its semantic weakening, witness and assertion are unchanged. Frozen prior
runner copies and evidence were not modified.

## Verified freestanding candidate (2026-11-10)

The corrected N42A candidate completed all 87 unique software suites:

- Tested source manifest: `b84446ef8a8e2b23f2e77c4d581069bfc6a8a4e33540351fcfbd45157c5c997c`.
- Source-only candidate archive: `frozen-n42a-source-b84446ef8a8e.tar.gz`, SHA-256
  `3cfa545c0438b984c214b0b13584919bf1b61d8619c5fb0a5e6f2c2992d44da7`.
- Exact inventory: the previous qualification-corrected 86 suites plus `nested42a`.
- Initial run: 2026-11-10T04:35:16.342486+00:00 to 2026-11-10T05:02:09.544915+00:00.
- Bounded continuation completed: 2026-11-10T05:09:41.900175+00:00.

The corrected run began with a clean, byte-verified copy of all 511 actual source
files and no copied build artifacts. It accepted 85 suites before `display_faults`
encountered missing UD/PF EFI images. That script assumes the documented
`make all smoke faults` preparation in `efi/README.md`; the clean runner had
omitted it. The normal framebuffer case passed, but the incomplete suite was not
accepted. This was a setup interruption before the UD guest launched.

An independent audit verified all 85 ordered zero-exit records, their exact log
segments, 171 guard records, both unchanged source trees and all 10,135 prior
artifacts. Those completed successes were retained. Before recovery, all 1,257
generated execution files were copied and verified. The separate continuation
built the documented prerequisites from the same source, then ran only the two
unfinished suites: all `display_faults` cases and `console_memory`. Both passed.
Its seven guards covered both 511-source trees and 11,945 protected paths.
The final combined ledger contains exactly 85 initial plus two resumed successes,
with no duplicate suite or acceptance reused across a source change.
The independent combined audit SHA-256 is `11ac70a7dce6b74719a4bc9c57df7eac2efbeed789f5002e1e2aa982f5451da1`.

The successful N42A invocation included 14 independent production cases, 24
assertion-rejected weakening mutants, and three separate GCC TSan cases, plus
implementation tests under O2/Werror/ASan/UBSan. It also compiled and linked the
existing N41C kernel using actual Makefile freestanding O1 flags and a nostdlib
link. The corrected atomic bridge had no unresolved libc memory helper and the
kernel no undefined symbols. Restoring only the previous large aggregate
initializer in a temporary copy reproduced its undefined `memset` link failure
as a required negative witness. The complete N41C suite separately rebuilt its
image and passed its TCG capability check.

The older candidate `1253b0146930` had accepted only N42A before its first full87
attempt exposed that initializer defect during the N41C kernel link. That failed
candidate, partial ledger and logs remain unchanged, separate evidence. None of
its acceptances was transferred to the corrected source. The six generated
entries in its older 516-entry manifest were excluded from the new actual-source
inventory, not deleted or modified in their original locations or old archive.

Initial evidence is under `efi/build/nested42a-full87-v2/`; continuation and
combined evidence are under `efi/build/nested42a-full87-resume/`. The interruption
and both phases are retained explicitly, not rewritten as an uninterrupted run.
This is host, freestanding-build and TCG acceptance. Physical CPU, VMCS reference
and INVEPT maintenance hooks still use mocks in N42A. There is no N42B runtime,
new VMX clear/retrack hardware acceptance, source-backing release or type-transition
claim. Prior hardware evidence and original kernel files remain untouched.

This section was appended only after the complete combined acceptance and
independent audit. No production or test source was changed afterward. The tested
pre-append manifest and frozen packages remain untouched. Documentation
before/after files, the exact append diff and hashes are retained under
`efi/build/nested42a-full87-resume/documentation/`.
