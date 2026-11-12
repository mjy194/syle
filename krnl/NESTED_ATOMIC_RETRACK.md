# N42B: one global A/D clear and private read retracking

Status: implementation and focused host/build checks. No N42B full-regression,
TCG, exact-image hardware or lifecycle acceptance is claimed here. N42A's
software coordinator acceptance and N41C's earlier hardware result remain
separate evidence.

## Bounded profile

`VMX_NESTED_ATOMIC_RETRACK_TEST` extends the N41C configuration without changing
its payload or acceptance marker. Sources remain immutable 4-KiB WB/IPAT1/AD1
catalog tables. There are two fixed physical owners, three source roots, a
budget of two private roots per owner and one maintenance round. A/B share
lower source tables; C has a separate target backing. Source backing remains
retained after terminal drain. No source replacement, memory-type transition,
DMA, migration or generalized VM lifecycle is implemented.

Build: `make -C efi smp-nested-atomic-retrack`, after sourcing
`efi/scripts/cloud-env.sh`. The ELF and ESP are separate from N41C.

## Production handoff

- `nested_owner_atomic_empty` initializes both owners without selecting or
  allocating a private root. `nart_empty_ready` release/acquire synchronizes
  those empty owners, binds the real N42A coordinator, then permits initial A
  selection. Existing N41C bootstrap remains empty-init followed by selection.
- `nart_round` obtains both phase-2 arrivals **before** `tam_close`. A lagging AP
  may still need normal fault/entry admission to reach its checkpoint.
- Domain maintenance uses the existing dispatch gate. The new synchronized
  `nested_domain_dispatch_maintenance_stopped` checks the complete owner token,
  complete AD_RESET maintenance ticket, exact target mask 3, physical stop and
  absence of pending local invalidation/internal resume state.
- BSP uses immutable ticket-bound arrival evidence for remote stopped queries.
  Only the actual owner executes VMCS/reference/INVEPT operations. The immutable
  command ticket is independent of the live owner epoch advanced by prepare.
- The production `nested_owner_atomic_maintenance` adapter injects only current
  VMCS read, VMPTRLD, EPTP VMREAD/VMWRITE, INVEPT and retained-map authority.
  It verifies VMCS01's exact audited AD0 EPT01, restores current VMCS02, parks it
  on that retained map and verifies readback before dropping its hardware-item
  reference. TAM releases metadata pins only afterward. No VMCLEAR is used.
- Reset retains the ordinary sparse flush and separately executes exact
  single-context INVEPT for every active/inactive private root. Receipts bind
  complete ticket, root identity/EPTP/HPA, owner/history and generation pair.
- Prepare pins the active root before restoration. A durable pending item is
  recorded before any potentially successful restore write. Uncertain write or
  readback retains that pin, pending reference and all launch bookkeeping.
- `open_entry` only completes physical gate bookkeeping. The outer epoch and
  owner release are published after successful `tam_finish`. Final publication
  and pre-commit abort share the metadata lock. A stale AP timeout rechecks the
  completed release; a real pre-commit failure prevents epoch publication.
- Any failed runtime handoff retains resources before generic guest cleanup.
  All N42B local fault sinks publish shared failure through
  `nested_parallel_mark_fault`; the peer's health and entry guards observe it.
  Metadata lock exhaustion never steals a lock or falls through to cleanup.

## Guest sequence and oracles

| Phase | CPU0 | CPU1 |
| --- | --- | --- |
| 0 | A read/write | A read/write |
| 1 | B read/write | B read/write |
| 2 | A warm read | C read/write |
| Maintenance | retained A/B, active A | retained B/C, active C |
| 3 | A read | C read |
| 4 | B read | B read |
| 5 | A new write | B read |
| 6 | B read | C new write |
| 7 | B new write | B new write |

The actual reason-48 path performs production CAS/refill/promotion and retries
unchanged faulting RIP. Checkpoints are after physical leave and vCPU complete,
before virtual return, with VMCS02 still current. Old/new target writes use
separate owner/key/epoch offsets and preserve the target's initial word.

At phase 2 both unique target source leaves must be D1. After the stopped clear
both must be A0D0, source pins remain four, four allocation identities remain,
and each private generation advances once. The successful changed-CAS count is
derived from the stopped source snapshot, not assumed equal to all entries.
At phases 3/4 executed reads must leave source targets A1D0 and all four private
target leaves physically RO/A1/D0. Phase 5 checks CPU1 B remains RO despite
CPU0's shared source D; phase 6 checks CPU0 B remains RO while C acquires D.
Phase 7 requires each B's own fresh promotion.

Expected bounded totals: nine unique source table pages, four exact reset
receipts, five terminal retirement receipts (one prior eviction plus four
survivors), nine target promotions (five initial plus four new), CPU0 two
creations/no eviction and CPU1 three creations/one eviction. Source/type and
allocation lifetimes do not advance. This is not a hardware CAS-collision test.

## Reproducible focused checks

- `efi/tests/nested42b.sh --host`: real two-thread production bootstrap,
  dispatch, source constructor, coordinator, adapter, bridge, private contexts
  and pools under privileged VMCS/INVEPT mocks; ASan/UBSan; shared failure hook;
  structural preprocessed integration; fresh actual freestanding ELF/ESP link.
- `efi/tests/nested42b_independent_review.py . --efi-prefix "$EFI_PREFIX"`:
  independent privileged-operation failures, all-four-root retracking, reopen
  probes, deterministic finish/abort race and compiling weakening mutants.
  Add `--sanitizer thread --skip-mutants` for the threaded sanitizer pass.
- `efi/tests/nested42b_integration.py`: structural wiring only, not execution.
- `efi/tests/nested42b_freestanding.py`: new source-only workspace, actual
  Makefile compile/link, required production flags, no libc/undefined symbols.
- `efi/tests/nested42b.sh --runtime`: explicit build and TCG CPUID_VMX0 skip
  check, with no N42B checkpoint or success marker permitted. Running this is
  a separate acceptance step and is not asserted by the implementation.

Independent production review, weakening mutants, final-source full regression,
immutable image/ESP freeze and separate two-CPU VMX acceptance must precede any
hardware completion claim. Preserve earlier N35/N41C/N42A evidence.
