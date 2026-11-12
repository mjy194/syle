# N47B: one stopped WB4K source replacement

This is an opt-in two-owner diagnostic, not generic dynamic EPT or permission
for source/DATA reclamation. Build `make smp-nested-source-replace` after the
existing cloud-env setup. Hardware execution is a separate authorized stage.

## Boundary

- Physical owners 0/1, WB 4KiB, IPAT1, AD1, three virtual roots. A/B share a
  lower source tree; C is an unchanged control. Fixed owner pools and metadata
  remain the original objects; closed predecessor banks and all stable slots,
  traces, receipts and allocation histories survive replacement.
- Exactly one A/B source leaf at EPT12 table GPA13000, index15, changes L2 GPA
  f000 from L1 GPAf000 (`backing.pages[14]`) to preowned GPA1d000 (`pages[28]`).
  C remains GPA1c000 (`pages[27]`). New leaf is 1d073, with A/D clear.
- All 83 source/data/fixed-map backing allocations remain retained, as do owner
  pool, VMCS and VMXON backing. Safely retired private EPT02 pages return only
  to their original pool. No PMM backing free runs in the N47B final path.
- No host PTE/EPT01/type/PAT/MTRR change, WBINVD, INVVPID, DMA or VM migration.
  Maintenance does not VMCLEAR and preserves both physical launch states.
  Only terminal successful diagnostic cleanup, after all guest phases, uses
  the pre-existing actual VMCLEAR/VMXOFF path before retaining the allocations.

## Physical checkpoint and gates

N47B has eight checkpoints, independent of N42/N43/N46. At every checkpoint the
physical leave, all private `nvmx_exit_store`, `nvmx_return_from_l2`, actual load
of VMCS01 and host reflection must have succeeded. Software `in_l2` is false
and the private model is not running before the checkpoint callback. The
source round begins after both owner arrivals at phase2. It never closes the
physical gate while the other owner still needs a fault/entry to reach it.

Each owner stays inside the round's parked command loop, touching no ordinary
registry/config API during rebind/rearm. The top of the real execution loop
checks the outer gate before any ordinary atomic/registry/config read. This
is a cooperative single-writer contract, not arbitrary concurrent API safety.

BSP calls the production `tsr` coordinator. Commands are RETIRE, PREPARE and
VALIDATE, each with release/acquire acknowledgement. The coordinator's internal
one-use command binds the actual key or stable full identity. The exact
SOURCE_REPLACE ticket is published before physical close. The provider uses
`tsr_consumer_stopped`, not the ordinary OPEN-only stopped query.

The owner adapter starts/ends at actual VMCS01, audits its parking EPTP, loads
VMCS02 and checks the exact old active EPTP. Park VMWRITE/readback succeeds
before precise unpin. Both active and inactive old roots use the real destroy,
local provider INVEPT, durable receipt, source-pin end and private-pool return
chain. New root pin precedes restore VMWRITE/readback. Both owners independently
recheck actual VMCS01/02 and unchanged launch state, publish full receipts and
stay parked. Domain OPEN does not release either owner: only exact
`tsr_released` plus successful local commit permits normal execution. Timeout
abort serializes with the same coordinator release and rechecks which won.
Uncertain hardware outcomes retain markers/pins/resources and stop.

## Source and alias proof

The old immutable `nested_atomic_memory_ready` remains unchanged. A separate
fixture validates generation-aware expected source words and both catalog
banks. The stopped management authority performs one expected CAS, comparing
all other raw source words INCLUDING A/D and all old/new/C DATA words against
the stopped snapshot. Source allocation lifetimes stay unchanged; all source
generations advance, while type epoch remains1. Source/updater nonce successors
inherit their independent final highwaters before publication.

The management graph now accepts unchanged source TABLE-backed leaf aliases
only if both leaf and TABLE span are exactly read-only, using TABLE authority.
Both old and new replacement destinations remain DATA-only. All five complete
fixed EPT maps, source RO guest aliases and every retained ownership identity
are audited. At phase2 each owner separately enumerates the full host CR3 via
`cache_page_audit_wb` for all83 backing pages: unique 4KiB RW/NX identity alias,
PAT0 WB and legal PAT bytes. Root/PAT must match between CPUs and remain stable
through park/restore. MTRR WB is checked separately. This audits current host
mappings, not nonexistent DMA or arbitrary other CR3s.

## Guest evidence

Keys:
- CPU0: A/B/A/A/B/A/A/B.
- CPU1: A/B/C/C/B/B/A/C.

Phases0/1 write distinct old AB slots; CPU1 phase2 also writes C. Replacement
occurs after phase2 return. Phases3/4 read new AB sentinel 5566556655665566 with
private RO leaves and source A1D0; C retains 99aa99aa99aa99aa. CPU0 phase5 first
write promotes its new private A; CPU1 phase6 separately first-writes A after
shared source D is already1. Phase7 verifies both new writes and C. Every
post-replacement stopped checkpoint checks all512 old and C words unchanged;
new DATA changes only the two authorized owner slots. Final expected totals:
7 target promotions, 11 created/retired stable slots, 4 old replacement root
retirements, 11 terminal receipts, zero live source pins after terminal cleanup.

Serial output includes checkpoint/current VMCS01, key/private EPTP/leaf,
source/catalog/allocation/cache identities, exact tickets, park/restore receipts,
local provider serial/ACK, host root/PAT and retained-backing summary.

## Focused validation

- `nested47b_owner.py`: real catalog/bridge/registry/pool/provider/dispatch/round,
  two pthreads, primitive failure and uncertain write/load results, same-HPA
  allocation reuse and ASan poisoned retired pool storage. VMX primitives mocked.
- `nested47b_memory.py`: real original/management/successor fixture, complete
  raw CAS checks, TABLE alias negatives, CAS injection and actual two-thread
  source begin/resolve/end/ready/A-D stress. The existing N47A memory suite was also run separately during
  development; it is not nested inside this runner.
- `nested47b_alias.py`: full-tree WB audit and existing cache-page contracts.
  TSan here is serial functional coverage, not a concurrency proof.
- `krnl/tests/nested_source_replace_payload.py`: original payload assembly plus
  relocated L2 instructions with VMCALL replaced by host returns, 256 orderings
  and20 negatives. Not actual nested entry or hardware A/D proof.
- `nested47b_terminal.py`: executes the entire selected production terminal tail,
  including inherited conditions, with legal two-owner reports and30 negative
  field/state variants. Restoring the old-sentinel contradiction is assertion-killed.
  Its plain, ASan/UBSan and TSan modes are serial host terminal logic, not VMX
  or earlier completion proof. `--source-root PATH --expect-legacy-contradiction`
  separately reproduces the exact v2 rejection without modifying its source.
- `nested47b_wiring.py`: preprocessed checkpoint/gate/retention integration checks.
- `nested47b_freestanding.py`: isolated real profile kernel/ESP build, symbol and
  no-hosted-runtime checks. Final PASS, both owner trace delimiters and retained
  marker must exist in allocated ELF sections and have executable code references.
  This prevents the optimized-away v2 terminal regression, but is not hardware
  reachability proof. No hardware execution.

Plain, ASan/UBSan and TSan focused runs are separate evidence. These checks do
not prove actual VMREAD/VMWRITE/INVEPT execution; exact-image two-mode Archbook
KVM evidence must follow an independent source review and separate authorization.


## v2 terminal failure retained, v3 correction

The v2 selected profile required the replacement sentinel in its owner report
then contradicted it with the inherited domain old-sentinel assertion. The
compiler removed the final PASS/trace reporting path. This was found during
acceptance preparation, before hardware. v2 source/image/evidence remain intact;
its earlier scoped software pass is not permission to use that image.

v3 leaves old profiles' domain assertion unchanged and excludes that assertion
only for SOURCE_REPLACE, whose complete owner report already checks the new
sentinel. DOMAIN_SHARED_READ still logs the verified value. The executable tail
reproducer confirms legal N47 data fails on exact v2 source and succeeds on v3;
reinstating the contradiction kills the v3 positive fixture. The final build
also checks actual allocated ELF emission references, not just source strings.


## v4 phase3 resident-count correction

The exact v3 checkpoint incorrectly required two resident contexts after every
nonzero phase. Source replacement retires both old contexts and rearms only
the current root. Both owners therefore have resident counts
`[1,2,2,1,2,2,2,2]` at phases0..7. Cumulative creation expectations remain
`[1,2,2,3,4,4,4,4]` and `[1,2,3,4,5,5,6,7]`. v4 changes only this count
predicate; it does not create an extra context or relax source/bridge guards.

`python3 tests/nested47b_checkpoint.py` executes the complete production
`atomic_checkpoint` body using real runtime types and controlled external
VMX/round/memory helpers. It checks both owners/all phases, count0..3, wrong
cumulative creation counts, range/null rejection, and the phase3 prior guards.
The exact v3 predicate mutant must fail the legal R6 phase3 count1 positive.
`--source-root PATH --expect-legacy-count` reproduces count1 rejection and
incorrect count2 acceptance on the untouched v3 source. Sanitizers are selected
with `--sanitizer address,undefined` or `--sanitizer thread`; this serial test
makes no concurrency or hardware claim. Hardware and full-suite acceptance
remain separate from this checkpoint correction and its focused verification.
