# N36A: domain-scoped multiprocessor typed-memory policy

N36A adds **pure software infrastructure only**. It does not alter
`cache_registered` or remove that service's single-CPU gate. No new EFI image,
PAT/MTRR change, UC access, INVEPT, INVLPG or WBINVD is performed by these modules.
The last verified N35C image remains unchanged.

The future profile is deliberately one dedicated data page, WB ↔ UC, IPAT=1,
AD=0, four-level 4 KiB EPT and WB table memory. Code, stacks, guest paging
structures, EPT/VMCS pages and communication buffers may not use the target.
WC, WP, PAT rewrites, arbitrary types and concurrent A/D are not enabled.

## Actual inventory scan, rather than an `all_aliases` shortcut

`domain_typed_inventory_audit` traverses every present path of every supplied
EPT root, including arbitrary PML4/PDPT/PD indices. All 512 words of each
visited table are inspected. Shared subtrees are visited separately for each
GPA path, so aliases are not lost through deduplication. The scan rejects
cycles, unowned or unaligned table views, the target as a table page, huge
leaves and unsupported attributes. Target aliases require WB or UC, IPAT1 and
no execute bit, including when an ancestor already denies execution. Other
leaf mappings must be WB. A caller-owned workspace avoids a large kernel stack.
Failure leaves the output unchanged.

Each root's ID, generation, exact AD0/WB EPTP and historical CPU mask accompany
its visited-node count, alias count and FNV scan digest. A never-used spare root
can have a zero historical mask; an old root with no remaining target leaf
still retains its historical residency. The digest detects accidental snapshot
mixing, but is neither cryptographic evidence nor proof of CPU cache state.
The scan only covers roots the caller enumerates. The domain owner must supply
all EPT01, EPT02, spare and historical roots and software refill sources. A scan
cannot discover a forgotten root or an unregistered DMA user.

Limits are explicit: 16 scanned roots, 16 target aliases and a caller-selected
node-visit budget. Budget/capacity failure is a safe refusal, not an assurance
of progress or a dynamic-allocation/OOM recovery claim. The proof assumes all
writers, including hardware A/D updates, are stopped throughout the snapshot.
No pointers are obtained by directly casting an untrusted physical address.

## Immutable lease inventory and pins

`domain_typed_lease` copies a trusted inventory containing domain/incarnation,
epoch, participants, host-PTE lease identity, root identities/generations/roles,
historical residency, raw and effective alias permissions, types/IPAT and
software refill IDs/generations. Alias counts must exactly match each root's
scan count, including zero. Root and refill generations must agree. The pure
model has explicit budgets of 16 roots, 32 aliases and 32 refill sources; the
current scanner's smaller alias budget remains the effective end-to-end limit.

The integration test assembles this inventory from **real scanner output**:
six EPT roots representing two EPT01, two EPT02, one unused spare and one
historically resident root, with five target aliases at distant GPAs. Omitting
an alias is rejected. The zero-alias historical root still requires both CPU
flush acknowledgements. `complete`, `tables_wb` and `dedicated_data_wb` remain
explicit external ownership/coverage attestations, not facts manufactured by
those Boolean values. Host CR3 alias scanning, per-CPU MTRR/PAT/CR0 verification
and target-role/physical ownership checks still belong to the runtime adapter.

Stable intrusive tokens bind owner, root index/generation, nonce, epoch and
type. Admission requires the exact live token and current OPEN epoch, IPAT1
and AD0. Existing pins remain held during a transition. They are not dropped
merely to make a reference counter zero while a VMCS might still reference a
root. All live tokens must individually prepare the new epoch/type before
admission reopens. Inventory mutation or addition of roots/refill sources
while this object is live is unsupported.

## Multiprocessor phase protocol

The BSP serializes the pure model; APs must report hardware results through
separate release/acquire mailboxes, not mutate this object concurrently.
Tickets bind domain, incarnation, new typed epoch, source and destination.
The caller first closes real consumer entry, obtains actual root quiescence
and drains updater work for the full participant set.

1. Detach every enumerated root's target aliases and revoke every software
   refill source. Every historically resident CPU must acknowledge its actual
   invalidation for each corresponding root before advancing.
2. Break the host mapping. Require every participant's TLB acknowledgement,
   then every participant's cache-maintenance acknowledgement.
3. Make the destination host mapping. Again require all TLB and cache ACKs.
4. Publish all destination-typed EPT roots/refill metadata and obtain every
   corresponding historical CPU invalidation acknowledgement.
5. Prepare all live lease tokens for the new epoch/type, then reopen admission.

BREAK/MAKE advance calls attest that the real saved host-PTE identity was
checked and the operation completed. ACKs record completed actions; the model
does not execute those actions. In the nonpresent window, no audit, log,
checksum or speculative use of the target is allowed. Runtime must compare
actual current mappings against the expected **current type and epoch**.
`inventory.alias.raw` is the initial WB snapshot, not permission to overwrite a
later UC mapping or reuse stale raw values during the reverse transition.

Wrong direction, stale/foreign ticket, duplicate ACK or wrong CPU cannot
advance the transaction. Explicit uncertain failure or timeout poisons the
lease permanently. Pins and inventory are retained; partial transitions are
not rolled back into runnable state. Return to WB must complete the same
protocol. Free readiness additionally requires actual retirement of every
VMCS/root reference, revocation of refill sources, all relevant invalidations,
zero pins and all participant CPUs parked. A local virtual L1 INVEPT conveys
none of this global transition or reclamation authority.

## Tests and remaining work

Run `efi/tests/nested36a.sh`. It builds only temporary `/tmp` executables with
O2, Werror, ASan and UBSan. Tests cover exhaustive distant aliases, empty
historical/spare roots, cycles/huge pages/type/AD/ownership refusals, scan
capacity/budget and unchanged outputs, raw/effective permission consistency,
missing alias/refill-generation mismatches, all-phase poison retention,
direction/epoch/CPU/duplicate ACK refusals, incomplete token preparation,
resource/nonce/epoch limits, and scanner-to-lease WB→UC→WB policy integration.

These are not hardware transition results. N36B still needs a domain-scoped
adapter that performs per-CPU preflight, real host and EPT alias changes,
maintenance dispatch, CD/NW trapping in all physical VMCS01/02 paths, exact
lease-backed typed resolver admission and full reverse-transition cleanup.

## N36B: bounded shared-domain hardware adapter (candidate)

The new `smp-nested-typed-uc` profile uses two pinned owners (APIC IDs 0/1),
one immutable four-level WB/AD0 EPT12 root and a dedicated shared data page at
L1 GPA `f000`. This is not a general multi-root typed-memory service. The old
single-CPU registered lease remains unchanged. No PAT or MTRR register is written.

The frozen root directory includes both EPT01 roots, both private sparse EPT02
roots, both never-installed prebuilt EPT02 roots, and the original fixture root.
The latter three have no historical hardware residency, but their complete trees
are still scanned and their target aliases changed. Historical owners of the
actual roots must execute local INVEPT both after detach and after publication.
The root lifetime identity remains fixed; sparse cache generations advance on
reset. Each reset removes every cached leaf and private refill record before the
current-type audit. A zero-alias root is not omitted from historical flush ACKs.

Before the first physical L1 entry, both owners have initialized their sparse
roots and publish local PAT/MTRR/CR0/CR3 proofs. BSP audits the unique host mapping,
constructs the seven-root inventory and acquires stable tree tokens. All actual
leaves use IPAT1. Only the registered data page may change WB to UC; all code,
stacks, paging/EPT structures and communication remain WB. Guest CD/NW changes
are trapped using CR0 masks in both physical VMCSs; all guest MSR accesses exit.

After each real L2 VMCALL, the owners stop in root. A separate typed-maintenance
purpose closes the domain entry gate. Owner-local sparse resets and flushes,
full current-type EPT audits, target-alias detach and historical-root INVEPT ACKs
precede the host break-before-make protocol. Both CPUs execute INVLPG and WBINVD
in each prescribed phase. No operation dereferences the target while its host
PTE is nonpresent. Publication changes the virtual EPT12 source and every fixed
EPT alias to the new type; empty sparse trees can refill only after both tokens
and owner epochs are prepared. The domain consumer gate opens last.

The reverse path re-audits the complete host tree for a current UC mapping and
all EPT roots for current UC aliases. The original WB raw PTE/digest is not a
reverse-transition authorization. Actual guest read/write evidence follows
WB warmup, UC operation at per-owner offsets, then restored WB operation using
the same VMCSs. This is an L0 coordinated transaction, not a broadcast meaning
ascribed to a virtual L1 local INVEPT.

Failure paths set atomic fatal latches and retain all resources. They do not
write the shared DTL ledger concurrently with a peer action, speculate rollback,
or reopen admission. Successful cleanup requires owner-local retirement INVEPT
while still in VMX root, after VMCS clears and sparse destruction; only after
both VMXOFF/park results may BSP retire ledger roots and release pins. Freeing is
permitted only in restored WB. The inventory and callbacks still rely on the
explicit immutable directory, stopped-consumer and private-RAM ownership
contracts. Digests are change detectors, not independent ownership proofs.

## N36B initial hardware failure (2026-11-09)

Candidate `f7e5ce526399fa3f422707842f6a26944c3815c26c6e6200e9da6dab0d739240`
failed both isolated archbook KVM two-CPU 256/512 MiB tests. After AP online
and BSP VMX root entry, both logs show `SMP NESTED BSP RUN FAILED RETAINED`
and panic, then timeout124. No N36B_PASS; no QEMU remained after cleanup.
Evidence `libfile_78ab52c13c2c81919bfb55cef5d4d53e`. Pure/TCG checks do not
turn this into a hardware success. Failure diagnosis is pending.

## N36B corrected hardware evidence (2026-11-09)

The old setup tail overwrote the new profile heartbeat leaf with IPAT0,
violating typed admission. The correction preserves IPAT1 for this profile,
keeps old profiles unchanged, validates the full final directory before
publication, and dumps only stable BSP-local failure trace.
Kernel `f66c52984b611e4d537337df72e061b648f28cc854bf42f3cde004f3be6fb6e4`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs show `SMP.N36B_PASS WB_UC_WB=1 nested_owners=2
all_roots_audited=7 typed_IPAT1=1 restored_before_free=1`, no error markers.
No QEMU remained. Evidence `libfile_b2f8a89a5a308191a529fa31bb93fc57`.
The initial failed hash and evidence remain separately recorded above.
This verifies the dedicated-data UC/IPAT1/AD0 profile, not arbitrary types,
guest structural writes, or A/D-enabled UC combination.

N36B corrected final independent full regression:
`efi/build/nested36b-fixed-review-full.log`, `FINAL_STATUS=0`,
72 marked tests plus console-memory; f66c5298 kernel unchanged.

## N37: explicit MP WT policy (candidate)

`smp-nested-typed-wt` uses the same seven-root/owner maintenance implementation,
not a second cache protocol. New opt-in APIs bind a non-WB policy of WT (4) to
inventory scanning, alias-plan preparation and its later validation, the DTL
ledger/tickets, and sparse typed admission. Legacy APIs still bind UC (0).
Neither policy permits direct UC-to-WT conversion, non-WB table memory, or free
while non-WB. The initial mapping and final retirement state remain WB.

Before allocations/AP startup the BSP checks PAT/MTRR capability and an existing
exact WT PAT slot. The AP independently proves target MTRR WB and its initial WB
host mapping before receiving a run request. BSP checks its own proof and equal
full PAT values/selected slot/encoding before entering VMX. Failure is explicitly
BLOCKED with no guest entry or cache transition; any already allocated resources
are retained. The first-entry barrier repeats local proofs after VMX setup.

All eight PAT bytes must be legal; slot zero must remain WB. The lowest existing
WT slot is selected, including 4K PTE bit 7 for slots 4 through 7. The host-current
WT audit is bound to that exact selection. Both CPUs keep PAT/MTRR unchanged and
CD/NW clear. MTRR WB combined with host PAT WT provides host WT, and guest EPT
WT/IPAT1 provides the same target type. Guest PAT cannot override IPAT1. Every
code/stack/table/mailbox mapping remains WB; EPTP remains WB/AD0. Reverse audit
checks the current WT mapping, not the UC slot-three rule or initial WB bits.

The actual read/write fixture and complete stopped-owner protocol are unchanged:
WB warm, WT per-owner-offset reads/writes, then restored WB verification, followed
by VMCS clear, retirement flush, VMXOFF/park and final pin release. No dynamic root
directory, WC/WP support, PAT/MTRR writes, arbitrary IPAT0 policy or multi-root
registry lifecycle is implied by this increment.

## N37 hardware evidence (2026-11-09)

Kernel `5c9a68b2a810d183fbda562bf9e12ece9e106d3e4aeb3c588eebf7028224c3e2`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs show `SMP.N37_PASS WB_WT_WB=1 nested_owners=2
all_roots_audited=7 typed_IPAT1=1 existing_PAT_only=1
restored_before_free=1`, without error markers. No QEMU remained.
Evidence: `libfile_2096b651ee408191a554b2278e4448d7`.
No PAT/MTRR writes or host configuration changes. Scope remains dedicated
4KiB data, IPAT1/AD0, WB tables, existing common WT slot, not WC/WP or
arbitrary guest type mutation/AD combinations.

N37 final independent full regression: `efi/build/nested37-review-full.log`,
`FINAL_STATUS=0`, 73 marked tests plus console-memory, kernel unchanged.


## N39A pure root-local retirement extension

The optional typed-root bridge now has a root-scoped retirement provider API
that permits unrelated owner directory changes while flush evidence is
collected unlocked. Durable receipts bind exact retirement nonce, type epoch
and commit version. The existing runtime still uses the serial provider;
this stage does not enable simultaneous owner churn or alter the whole-domain
WB/UC transition protocol. See `TYPED_ROOT_LEASE.md` and the pure
`efi/tests/nested39a.sh` actual-registry interleaving and negative-witness suite.
Root-tree retirement, including in UC, remains separate from backing release.
