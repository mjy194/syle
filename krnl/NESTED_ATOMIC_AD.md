# N35A: immutable EPT12 atomic A/D substrate

This is independently executable software, not a VMX runtime integration.
N34C2 and all existing serialized A/D paths remain unchanged. N35A does not
establish arbitrary concurrent L1 page-table mutation support.

## Supported contract

Four-level, 4 KiB, WB EPT12 with EPTP.AD=1, no execute-only or large pages in
this new first increment. Tables are aligned, owned, pinned WB RAM. Every guest
alias to these structural pages is read-only; the L0 authority to atomically
update A/D is separate from guest EPT01 write permission. Resolver callbacks
must prove this contract and the fixed backing lifetime; returning a pointer
is not itself a physical alias audit. Ordinary guest/host structural writes
are forbidden for the whole open generation. All concurrent accesses to the
shared entry words, including trace sampling and identity revalidation, use
consistent `__atomic` 64-bit operations. Default backend requires lock-free
64-bit atomics; injectable operations must obey the same linearizable contract.

`nested_ept_atomic_walk` reads a private per-call trace and validates the full
path and backing. Before each update it re-resolves the same table GPA and
checks its HPA and host view. It compares the non-A/D identity and CASes only
`current | this_operation_bits`. Nonleaf requests only A. A successful write
requests leaf A|D; a read never replays D remembered in an earlier sample.
A CAS collision reloads and retries, bounded at 64 attempts per entry.

Nonpresent terminals are not updated; a validated present prefix can gain A.
A legal permission-denied complete path gains A but not new D. These are
explicit deterministic allowed software effects, not a claim to reproduce
native microarchitectural failure ordering. A complete leaf's L0 backing
failure takes precedence over a virtual L1 permission denial and stops.
Unsupported formats fail closed rather than fabricate reason 49.

Only `status == NEA_DONE` together with `translation.status == NESTED_EPT_OK`
authorizes use of the translation. Failure may retain an otherwise populated
translation structure, and may have changed a prefix. `changed` means this
call performed at least one value-changing CAS. `NEA_STALE && changed` is an
applied-but-stale result requiring retention, not rollback or normal reentry.

## Actual updater admission and drain

`ad_updater_gate` uses one bounded atomic metadata lock to linearize acquire
and close. A close cannot observe zero writers while an older acquire later
increments the count. Each live lease has a stable intrusive token and an
independent saved monotonic nonce, bound to domain/incarnation/context/epoch/
lifetime. Same-address, same-epoch token reuse cannot accept an old release.
Token storage is exclusive to its owner and cannot be moved or probed through
an unrelated concurrently active gate. Nonce and epoch overflow refuse progress.
The metadata lock never spans a walk, CAS, callback or wait for another writer.

A closed gate rejects new leases but permits existing valid leases to finish.
Only closed plus drained authorizes the caller to start clear/reset maintenance.
Reopen requires a new epoch and the caller's completed all-consumer invalidation
attestation. This Boolean is an explicit proof obligation, not execution of
INVEPT. The guest-consumer entry gate is separate: updater count zero does not
mean no CPU is running a guest or has cached an EPT02 translation.

`nested_ept_atomic_walk_leased` acquires before resolving any table, composes
the live lease guard with the trusted ownership guard, and releases after the
whole walk/update. Session storage is caller-owned and must survive a failed
release. A lock failure poisons the gate; the token and physical table pins
remain retained until cleanup can really release them. Poison is not a magical
revocation of an operation already in flight. Post-CAS checks report its effects.

## Required later runtime protocol

Before clearing shared A/D, stop both actual consumers in root and close their
entry gate, close updater admission, drain every updater, invalidate all owner
EPT02 caches and traces with real successful owner INVEPT acknowledgements,
then clear the shared bits and publish a new epoch before reopening. Retain all
backing through this sequence. Local L1 INVEPT alone cannot authorize global
clear or reclamation. No old generation may refill or promote a cached leaf
after maintenance. Physical EPT02.AD=1 and RO-to-W dirty tracking must still be
integrated and tested, including guest paging-structure accesses treated as
writes. No such hardware integration is claimed by N35A.

CAS is not a four-level snapshot, does not prevent structural ABA/remapping,
and cannot make ordinary guest stores participate in this protocol. Structure
immutability and lifetime pins are essential external preconditions, not
properties inferred by an epoch check. Multiple roots, concurrent L1 structure
stores and typed backing are not newly enabled.

## Evidence

Run `efi/tests/nested35a.sh`. It builds only temporary `/tmp` executables with
O2, Werror, pthread, ASan and UBSan. Tests cover injected conflicting CAS,
identity/HPA changes, bounded retries, stale checks before/after mutation,
nonpresent prefixes and partial effects, eight concurrent live gate leases,
close/drain, same-storage nonce ABA, poison and overflow. An integrated real
atomic test runs two walker threads over the same leaf (read and write), closes
the gate while both are admitted, verifies rejected new work and preserved old
leases, then observes A/D merge. A drained epoch transition clears bits and a
read sets A without resurrecting old D. An injected stalled metadata owner
proves failed release retains caller session storage and poison blocks reuse.

Tests do not require the scheduler to produce a CAS collision: forced conflicts
are separately deterministic. There is no new EFI image, VMX execution or
hardware proof in this increment. The preceding C2 kernel is unchanged.

## N35B1 candidate: real shared-root merge (hardware pending)

Build `make -C efi smp-nested-atomic-ad`; focused checks `efi/tests/nested35b.sh`.
The candidate reuses the shared L1 domain and two owner-private sparse trees,
but selects one immutable virtual EPTP `0x1005e`. Only this new profile exposes
virtual EPT A/D capability and requires the physical bit-21 capability. Both
actual EPT02 roots have AD enabled; EPT01 remains AD0. The capability check is
performed locally before tree initialization on both CPUs. No multi-root,
typed backing, L1 structural writer or clear-A/D round is added.

Before either CPU runs, `nested_domain_memory_prepare_atomic_ad` audits unique
WB backing, both full EPT01 maps and original prebuilt EPT02 maps, and all four
EPT12 table pages. Table GPA 0x10000..0x13000 has only RO guest aliases. The BSP
then binds a shared updater gate and immutable ownership stamp to the two
owner adapters. Runtime table resolution verifies the same owned HPA and RO
EPT01 mapping using atomic loads; its WB host write authority derives from
this prepublication proof, not from granting guest table write access. Guest
CR3 pages (including CPU1 GPA 0x21000..0x24000) remain distinct and writable.

A local cache miss allocates its trace record from the preallocated owner pool,
then holds a real updater lease through atomic walk, CAS and EPT02 publication.
Read mappings with write permission are initially installed with a separate
RO tracking restriction even when another owner already set shared D. A later
write fault checks the cached stamp, table identity, HPA and permissions before
any new CAS, then sets D atomically and promotes the actual EPT02 leaf only
with successful local INVEPT. A cached fault not explained by this tracking is
fatal. Physical AD1 page-walk exits must carry read+write access when identified
by qualification bits 7/8; all requested access bits are checked. This is not
the old serialized A/D update API.

Both real L2 instances read GPA 0xf000 and meet the mutual heartbeat barrier.
CPU1 then writes only offset 8; CPU0 reads only offset 16. The original offset
0 remains unchanged. The final check requires used-path A, shared leaf A|D,
exact data sentinels, and target-page promotions 0/1 for CPU0/CPU1. This proves
shared updates and local first-write tracking, not that a hardware run happened
to schedule two CAS instructions simultaneously. Forced CAS conflicts remain
covered by N35A's deterministic tests.

Each owner clears its VMCS02, destroys/flushes its private tree, and releases
only its private trace records. Shared tables remain alive while the other
owner may run. BSP does not close/drain the shared gate or free shared backing
until both owners have actually cleared VMCS01/02, left VMX, published success,
and AP is parked. Any failure retains resources. This is final teardown, not a
reusable all-consumer clear-A/D transaction. That second round remains a
separate future increment and must include real consumer gates and flush ACKs.

Pure owner-ad tests cover tracked read→write, cached identity mismatch before
CAS, local flush failure retention, and safe private-record cleanup. The focused
TCG run only verifies the legitimate missing-VMX skip; KVM acceptance requires
`SMP.N35B1_PASS` and its A/D/promotion/data evidence, not `SMOKE_PASS` alone.

## N35B1 hardware evidence (2026-11-09)

Kernel `129c68b51d0fbf5830fdb7d3c7f15a893dd1f49195f43b395545a5b9eebffc58`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs contain `SMP.N35B1_PASS immutable_shared_AD1=1 private02_AD1=2
atomic_merge=1 target_firstD_trap=1 updater_drained=1 no_clear_cycle=1`,
without error markers. No QEMU remained. Evidence:
`libfile_803b57d8c0a481918e00d03c54d4f4f0`.
This verifies the restricted immutable single-root merge profile, not a
clear/retrack cycle or arbitrary concurrent guest structural writes.

N35B1 final independent full regression: `efi/build/nested35b-review-full.log`,
`FINAL_STATUS=0`, 70 marked tests plus console-memory, kernel unchanged.

## N35C candidate: all-consumer reset and read-only retracking

Build `smp-nested-ad-reset`; focused checks `efi/tests/nested35c.sh`.
This candidate retains the single immutable WB/AD1 shared root and fixed owner
CPUs. It adds two **cooperative actual-root** checkpoints after verified L2
VMCALL exits, not forced exit of two running guests. Owner arrival is published
only after actual physical consumer leave. The other owner may finish its
current interval; neither arriving owner authorizes another entry while waiting.

The pure domain controller has a separate MAINTENANCE gate and complete tickets
(domain, incarnation, maintenance epoch, purpose). AD_RESET and AD_OBSERVE are
distinct purposes; neither grants backing reclamation authority. Begin requires
all participants virtual-root, no physical consumer running/prepared/pending,
and snapshots historical residency. ACKs precede metadata publish; publish
requires every target ACK; complete requires publish and opens consumer entry.
Wrong-purpose, old/duplicate/foreign tickets cannot complete another operation.

RESET sequence:

1. Both actual owners stop at the checkpoint. BSP closes consumer admission
   under the dispatch lock, then closes updater admission and verifies drain.
2. Each owner resets its existing sparse root and executes real local INVEPT.
   Only after success does it free its private trace/promotion source records.
   EPTP/root identity and physical VMCS launch state are preserved. Owner ACKs
   are published only after these operations finish.
3. After all ACKs, BSP verifies previous target D=1 and atomically clears A/D
   on present shared EPT12 entries. No guest, hardware EPT02 writer or software
   updater can still mutate them. Shared backing remains owned, not freed.
4. Both owners independently prepare the new updater stamp while the old gate
   is closed/drained and their private trees are empty. AP sends a separate
   prepare ACK. BSP opens the updater gate at the new epoch only after both
   metadata sets are ready, then publishes/completes the maintenance ticket.
   Consumer entry opens last. Resume uses the existing VMCS and VMRESUME, not
   a hidden VMCLEAR/relaunch or new-root initialization.

Next both L2s only read the target data page and exit again. AD_OBSERVE closes
consumer admission with a fresh purpose-specific ticket. Because both owner
fault handlers have synchronously returned and there is no asynchronous updater
source, a locked idle-query proves zero updater leases without changing epoch.
The query alone is NOT a synchronization barrier. This stage neither resets
caches nor closes/reopens updater admission. BSP requires target A=1,D=0 before
releasing the observation checkpoint. Other leaves used as guest paging pages
may already have D=1 and are not incorrectly required to remain clean.

The final interval lets only AP write the target's separate offset again. Its
new read-installed tracking leaf must fault, CAS D and locally invalidate before
promotion. Final evidence requires target D=1, cumulative target promotions
0/2, updater epoch 2 and the intermediate exact read-only leaf `0xf133`.

Any timeout, unexpected owner state, reset/flush failure, stale ticket, failed
stamp preparation or admission mismatch poisons both protocols and retains
resources. Partial resets are not rolled back. Clear followed by failure never
authorizes reentry. The reset helper does not silently advance a stamp or free
records when flush failed. Old tokens/traces cannot cross the new epoch.
This is L0 global maintenance, not implicit cross-CPU semantics of L1 INVEPT,
and still does not allow concurrent guest structural stores or multiple roots.

## N35C hardware evidence (2026-11-09)

Kernel `97fa3d2f02377afc5e9766c352126cbba259dbfd295916fb1cd7641e3098172b`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs show `SMP.N35C_PASS owner_reset_INVEPT=2 updater_epoch=2
readonly_A1D0=1 new_write_D1=1 same_VMCS_resume=1
no_backing_free_authority=1`, without error markers. No QEMU remained.
Evidence: `libfile_0b56b16a10008191a3586c5ec8a53030`.
This is controlled single-root maintenance with cooperative real root
checkpoints, not arbitrary L1 stores or a backing reclamation authority.

N35C final independent full regression: `efi/build/nested35c-review-full.log`,
`FINAL_STATUS=0`, 71 marked tests plus console-memory, kernel unchanged.

## N41A separate atomic catalog substrate

`NESTED_ATOMIC_CATALOG.md` describes the new pure AD1-only WB/IPAT1 source
catalog, explicit writable L0 authority, whole-operation source/updater permits
and fully scoped trace validation. Old single-root runtime profiles are not
rewired. Multi-root VMX, persistent trace revocation and AD1 retirement remain
separate increments; no new hardware acceptance is claimed here.
