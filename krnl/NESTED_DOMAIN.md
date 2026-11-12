# N33A: pure nested-domain policy, not hardware integration

This increment does not alter the frozen N32B image or establish that N32B
works on hardware. It introduces a separately tested ownership protocol;
its operations do not execute VMX, publish page tables, send IPIs, flush caches
or make the existing nested adapter concurrent-safe.

## Distinct ownership layers

A virtual CPU owns virtual VMXON/root/current state. A VMCS12 object owns its
field storage identity and launch/active/entry lifecycle. Changing a virtual
CPU's current object does not deactivate its previous object. Another owner
cannot acquire that object until the first owner has cleared and detached it.
The first profile requires all objects to be cleared before virtual VMXOFF;
this is a deliberate policy restriction, not a newly invented Intel error.
Object handles contain domain identity, incarnation and allocation generation
so a stale handle cannot affect a replacement at the same guest address.
The caller must never reuse an incarnation for a new domain lifetime.

VM-entry issue and observed outcome are distinct. A normal observed exit
confirms that entry succeeded; failed entry must preserve the earlier launch
state. The model records knowledge of the outcome, rather than claiming that
architectural launch state changes only at exit. It does not implement VMX
instruction exception precedence or assign architectural VMfail error codes.

## Two different invalidations

A virtual INVEPT request describes one virtual CPU's cache scope. Type 1 names
an EPT12 root; type 2 names all contexts for that virtual CPU. Its completion
does not acknowledge an L0 domain-wide backing revocation. A future adapter
must decode and validate real INVEPT operands and perform actual physical
invalidation before reporting completion.

L0 resource revocation instead concerns every consumer that may retain a
translation. Its target set includes historical residency, not just current
running CPUs. Guest execution includes L1 as well as L2: L1 may reach the same
backing through EPT01. The controller closes entry, requires actual owner
quiescence, publishes the change, and accepts epoch-tagged acknowledgements
only after corresponding owner-local invalidations. Completion requires the
whole target mask. Acknowledgement counters alone are insufficient. Failure
freezes the gate and retains resources rather than guessing rollback safety.

The pure API's publication and ACK calls are caller attestations, not proof of
CPU behaviour. No backing may be freed merely because a unit test passed.

## Intended hardware progression

After N32B hardware validation, introduce a shared L1 GPA domain with private
per-vCPU VMXON/VMCS12/stack/architectural CR2 state and owner-private EPT02
registries. Start with WB, A/D disabled, pinned owners, no concurrent guest EPT12
writes and no shared typed lease. Then integrate stopped-root domain revocation
with all owner-local flushes and real resumed reads. Running-guest kick and
shared A/D/table mutation protocols are separate later work.

This module is serialized controller policy. It does not turn the existing
PMM, LRU registry, pin counters, A/D writeback or typed backing service into
thread-safe structures. A host lock cannot by itself stop guest writes to
shared EPT12 tables.

The initial bounded policy uses a fixed virtual-CPU-to-owner-CPU association.
A residency bit therefore identifies that pinned consumer. It is not a
migration protocol: moving a vCPU between physical CPUs would require retaining
and flushing every old physical owner separately. Resource-budget exhaustion
is a policy/OOM result, not an architectural VMX failure. Unit tests must cover
stale object reuse, duplicate releases, unknown consumers, incomplete ACK sets,
failed entry and sequence overflow without mutating unrelated live state.

`reclaim_publish` attests more than a single EPT02 leaf write or IPI send:
all sources capable of refilling the revoked backing must already have been
removed or redirected. This includes the domain GPA mapping, EPT01, every
consumer's EPT02 software cache/plan and their relevant generations. Otherwise
a successfully flushed CPU could simply refill the freed HPA from stale
software state after the gate reopens. The pure model cannot inspect these
sources. Actual backing release is forbidden before successful transaction
completion, and requires the caller to have fulfilled this entire publication
contract, not merely collected a numeric ACK count.

## Independent verification (2026-11-09)

`/tmp/nested33a-independent-review.log`: ASan/UBSan/-Werror base suite and
additional launched+early-fail preservation, stale transaction ACK, same-domain
new-incarnation ABA and poison-state mutator rejection checks passed.
No hardware path or full build was modified. Frozen N32B kernel remains
`35664b81d2906fdf740dc7ae94e7ab5138293c83462f517e6a15e930faca7c2c`.
Archbook remains offline; N32B KVM and N33 shared-domain integration are unverified.

## N33B bounded hardware adapter

`make -C efi smp-nested-domain` and `efi/tests/nested33b.sh` add an opt-in
trusted two-owner adapter. Unlike N32B's independent domains, every mapped
GPA has exactly the same HPA and permissions in both EPT01 roots. The bounded
map contains GPA pages 1 through 0x3f; page zero and all other addresses are
unmapped. A single backing owner retains/frees the pages; per-vCPU fixture
views borrow them and must never call ordinary fixture destruction.

| Resource | CPU 0 GPA | CPU 1 GPA | Guest virtual address |
|---|---|---|---|
| Four guest paging levels | 1000–4000 | 21000–24000 | CR3=1000 / 21000 |
| GDT, IDT, TSS page | 5000 | 25000 | 5000, 5800, 5100 |
| Code | 6000 shared | 6000 shared | 6000 |
| L1/L2 stack backing | 7000–9000 | 27000–29000 | unchanged stack addresses |
| Rendezvous | a000 shared | a000 shared | a000 |
| L1 bootstrap/config | c000 | 2c000 | c000 |
| Virtual VMXON | d000 | 2d000 | VMX operand contains actual GPA |
| Virtual VMCS12 | e000 | 2e000 | VMX operand contains actual GPA |
| Common read data | f000 shared | f000 shared | f000 |
| EPT12 tables | 10000–13000 shared | same | shared GPA root 1001e |

All addresses in the table are hexadecimal. The VMX operand's effective
address is translated as GVA, but the pointer value read from that operand is
already GPA and is not translated or offset a second time. CPU 1's guest page
tables map its fixed low virtual addresses to different private GPAs; the
GPA-to-HPA domain itself never changes depending on CPU.

BSP builds the shared EPT12 before AP execution and maps those table pages
read-only for guests. EPT12 and EPT01 permissions are composed by the existing
pure walker into two distinct, preallocated, L0-owned EPT02 roots. Code is RX,
ordinary private data is RW/NX, and EPT12 table pages remain read-only. WB/4K,
A/D disabled, no demand fills, typed backing or concurrent table changes.
Every owner invalidates its own physical EPT02 before use and after clearing
VMCS02; EPT01 is separately invalidated by its existing owner cleanup.

The shared owner model is connected to actual decoded virtual VMX operations,
not pre-registered success records. A bounded atomic metadata lock protects
candidate domain and private nvmx state transitions; both candidates commit
only if consistent. VMXON, VMPTRLD, VMCLEAR, VMXOFF and entry requests enforce
fixed assigned object ownership. Initial inactive VMCLEAR is allowed without
inventing an implicit VMPTRLD. Physical L1 and L2 entry/exit each register a
consumer transition. A real normal L2 exit confirms launch exactly once;
the old adapter commit is disabled in this profile. Unexpected physical entry
failure or ownership inconsistency stops and retains resources.

The lock uses PAUSE and a finite attempt budget, not a wall-clock timeout.
No VM entry, VMCS hardware operation, allocation, logging, flush or waiting for
another CPU occurs while holding it. A fault latch does not magically stop a
CPU already authorized to enter; failure cleanup must still retain resources
until real owner stop is known. The lock does not prevent ordinary guest stores
and is not a solution for writable shared EPT12 or hostile guest concurrency.

Both L2s really read GPA f000, record its expected value in their separate
rendezvous slots, and complete the mutual execution-interval barrier before
VMCALL. BSP verifies both results, domain final ownership state, both physical
VMCS clear/OFF completions and AP park before releasing shared backing. CPU
GPR/CR2 sentinels and full local host-state checks remain enabled. This is not
a complete SMP L1 OS, VMCS migration, runtime shared-map revocation, remote
virtual INVEPT broadcast or concurrent A/D implementation. Runtime INVEPT
requests outside this immutable profile are rejected, not guessed successful.

## N33B hardware evidence (2026-11-09)

Corrected kernel `bbf8eb24ae3ad0cd429e1e143be003c97c4fdbc8af767b4611ac6948a89e9c9d`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Each run shows both L2 reads `0x3344334433443344`, two real N1 launch/resume
and context-owner proofs, and `N33B_PASS shared_GPA_domain=1 shared_EPT12=1
private_EPT02=2 actual_dispatch_owner=1 L2_shared_read=2`, with no failure or
timeout markers. No test QEMU remained. Evidence Library:
`libfile_ed278802834481918f31a6c142b31b30`.
The earlier 76b6 candidate was not hardware tested: review caught CPU1's final
counter check reading fixed GPA c820 rather than config_gpa+820 (2c820).
Scope remains controlled WB/AD0 shared-domain guests with fixed ownership and
read-only EPT12, not arbitrary guest mutation, migration or cross-CPU virtual
INVEPT broadcast semantics.

N33B final independent full regression: `efi/build/nested33b-review-full.log`,
`FINAL_STATUS=0`, 62 marked tests plus console-memory. Final kernel unchanged
from both hardware runs; paused IOAPIC remains excluded.

## N33C: stopped-owner L0 backing revocation

This adds a distinct L0 safety transaction, **not** broadcast semantics for a
virtual L1 INVEPT. Both L2s first read GPA f000 from the old shared backing.
After their first verified VMCALL exit, physical consumer leave and virtual
entry commit have completed. They rendezvous in root before returning to L1.
The BSP closes the domain entry gate with the full reclaim ticket; targets
include both historical consumers, not just a currently selected VMCS.

A replacement WB page is allocated before AP execution. While both owners are
stopped, all four EPT01/EPT02 leaf arrays and their topology are validated,
including absent entries and aliases. The limited current layout requires
exactly one old-data GPA (f000) per root and no unexpected aliases. The
authoritative backing array, four leaves and both borrowed fixture snapshots
are updated before publish. EPT12 remains an immutable GPA identity mapping;
there is no surviving software plan which can refill the retired HPA.
Each owner executes local single-context INVEPT for its own EPT01 and EPT02,
then ACKs the full domain/incarnation/epoch ticket. Only all-consumer completion
opens entry. The AP's release/acquire completion mailbox does not substitute
for the ticket ACK or the actual instructions.

The existing VMCSs remain current/launched. Resume goes through the ordinary
L1 VMRESUME path, without an initialization flush or VMCLEAR between reads.
Both L2s read new value 5566556655665566 and record it independently. Old HPA
ownership is retained through the entire transaction, and freed only after
both owner CPUs have cleared both VMCSs, left VMX and parked. Failure/timeout
latches the domain, retains all backing and never attempts speculative rollback.
The waiting budget uses TSC cycles, not calibrated seconds.

Scope: one stopped-root WB/AD0 shared domain, two fixed owners, prebuilt private
EPT02 roots, one validated data GPA replacement. This is not running-guest
revocation, arbitrary alias discovery, nested concurrent table mutation or
general resource reclamation. Both CPUs' physical exits are required; guest
heartbeat alone is insufficient. Pure tests cover failed prevalidation,
ownership/alias collisions, full-ticket stale/duplicate/unknown ACK, historical
targets and resumed dispatch. Hardware validation is separately required.

## N33C hardware evidence (2026-11-09)

Kernel `65b2e44bd9ec3300ddee8d8cc0bc547680a734f7c46c7ba04468f3ffc3442005`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Each run records two old 0x3344334433443344 reads and two new
0x5566556655665566 reads, with `N33C_PASS stopped_owners=2 historical_targets=3
local_INVEPT01_02=2 new_backing_reads=2 retained_until_clear_off=1`.
Both owners reached actual L2 exits before publication; each owner invalidated
its own 01/02 context and acknowledged the complete domain transaction ticket.
No temporary QEMU remained. Evidence Library:
`libfile_23f7d8fc04a08191875b5032f75a998a`.
This is one stopped-root L0 revocation transaction for an exact GPA, not L1
INVEPT automatically broadcasting, arbitrary mappings or live-writer coherence.

N33C final independent full regression: `efi/build/nested33c-review-full.log`,
`FINAL_STATUS=0`, 63 marked tests plus console-memory; final kernel unchanged.

## N33D: one running nested owner kicked into the L0 transaction

Only AP L2 is kicked. BSP has already reached its first real L2 VMCALL exit and
coordinates from root. AP L2 runs with IF=0, publishes its heartbeat and loops
without a voluntary exit. BSP sends physical xAPIC fixed vector41. Both physical
VMCS01/02 request external-interrupt exiting and acknowledge-on-exit. L0 accepts
only AP/in-L2 reason1, exact valid external vector41, IF0, no blocking/vectoring,
and RIP within the trusted heartbeat/wait interval. The interval includes the
heartbeat publication's following fence so an immediately delivered kick is
not incorrectly rejected. ISR41 must be the sole in-service vector; EOI must
clear it. Root delivery, other vectors or phases are unsupported/fail-retain,
not routed to L1. AP runs from main, not a still-in-service IPI handler.

An internal-pause bridge confirms first entry exactly once and leaves the
physical consumer while retaining the virtual L2 running state. It records
pending/resumed ownership separately. This allows the stopped-root reclaim
transaction without pretending a virtual VM exit happened. Internal resume
registers the physical consumer only, without prepare/start/commit again.
The later normal VMCALL consumes pending state and performs no second commit;
only the adapter's normal VMCS12 snapshot/return ends the virtual entry.
No exit length is read or added for the physical external interrupt.

After both owner flush ACKs and transaction completion, BSP releases the
guest loop. AP reenters its preserved physical VMCS at its original RIP/GPR
state, reads the new backing, and then issues its normal VMCALL. Both owners
also execute the ordinary second L2 read. Old backing remains retained until
final owner clear/OFF/park. Pure bridge tests cover repeated pause/resume,
closed-gate resume refusal, unknown exit/entry-failure poison, full ticket
completion and subsequent ordinary virtual VMRESUME.

This is one AP running-L2 kick plus one root coordinator, not two simultaneously
kicked owners, arbitrary L1/L2 event routing, guest-visible interrupt injection
or unrestricted concurrent shared page-table mutation.

## N33D hardware evidence (2026-11-09)

Kernel `4170433084cbd2596dd88d84a342e1bfaf9727821c790883ca81c4c3fb382f3d`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs record actual L0 kick reason1/info0x80000041 with guest IF0 and
original RIP0x61bc, then `N33D_PASS AP_L2_actual_kick=1 BSP_root_coordinator=1
L0_owned_IRQ=1 original_RIP_resume=1 new_backing=2`, with no error markers.
The L0 IRQ was consumed internally, not reflected as an L1 guest interrupt;
only the following normal VMCALLs were reflected. No temporary QEMU remained.
Evidence Library: `libfile_e523cd6c6164819189b0ebca9efd9a1b`.
Scope is one running AP L2 and BSP root coordinator, fixed owners and mapping;
not two simultaneously kicked owners or arbitrary running L1 interruption.

N33D final independent full regression: `efi/build/nested33d-review-full.log`,
`FINAL_STATUS=0`, 64 marked tests plus console-memory; final kernel unchanged.

## N34A: per-owner sparse EPT02 foundation

Two fixed owners now use independently allocated sparse EPT02 trees, initially
empty. Existing prebuilt EPT02 storage remains owned but is never installed for
this profile. Shared EPT01/domain RAM and immutable EPT12 remain unchanged.
BSP allocates 64 private WB pages per owner before AP execution. Every sparse
node and leaf-metadata allocation is from that owner's pool; callbacks never
call PMM. Runtime validates the executing APIC owner. Pool exhaustion and
flush failures stop with retained resources. Only after both VMCS pairs are
cleared, both CPUs leave VMX, and AP is parked does BSP detach empty pools and
free their original backing pages.

The strong pool token includes owner/incarnation/slot/generation and rejects
stale/double frees. The sparse allocator interface carries only HPA/view, not
that token. Its callback contract is deliberately narrower: synchronized,
exactly-once frees from the trusted sparse core, no delayed copied free across
reuse. This interface cannot independently prove ABA safety for an arbitrary
malicious callback caller.

Each real reason48 uses the hardware GPA and full access mask. Cache HIT
followed by an allegedly permitted fault is rejected instead of refilling in
a loop. A MISS walks EPT12 through the shared owned backing/EPT01 permissions,
preserves L0 backing-failure precedence, and only installs successful mappings.
This first stage fails closed on L1 denial or unsupported format; it does not
claim general reflected-fault routing. Shared tables are immutable during
walks. No A/D writes, non-WB types, or concurrent guest table updates.

Internal EPT suspensions may occur repeatedly. First successful physical
entry commits virtual launch once; subsequent misses leave/reenter only the
physical consumer while virtual L2 remains running. VMCS12 guest state is not
copied on internal misses. The eventual true VMCALL reflection ends the virtual
entry. Pure lifecycle tests cover repeated misses, gate closure, overflow,
wrong origin and physical failures. Real dual-owner lazy fills and shared-data
reads require KVM verification. Virtual local INVEPT/mutable EPT12 is the next
separate increment, not implemented by this foundation.

## N34A hardware evidence (2026-11-09)

Kernel `b70d7e20b5c2c02069c728e1a2ff7377cf6f711349df3ea0bfbc065e9abf79f8`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Each run records 18 real OWNER_EPT_INTERNAL_FILL events and
`N34A_PASS private_owner_pools=2 real_lazy_EPT02=2 shared_L2_read=2
repeated_internal_resume=1`, with no failure/timeout markers. No QEMU remained.
Evidence Library: `libfile_caccc4f6a470819188c93cfbe5c6273a`.
This verifies private preallocated per-owner pools and initially empty sparse
EPT02 with repeated internal misses, not L1 local INVEPT or mutable EPT12 yet.

N34A final independent full regression: `efi/build/nested34a-review-full.log`,
`FINAL_STATUS=0`, 65 marked tests plus console-memory; final kernel unchanged.

## N34B: native L1 owner-local INVEPT

After both first L2 reads and verified physical exits, AP stops in root while
BSP's L1 is the only EPT12 writer. EPT01 permits L1 writes to the EPT12 PT page;
L2's corresponding EPT12 leaf remains read-only. Native L1 changes the f000
leaf to L1 GPA b000, whose distinct WB backing is retained for the entire run.
Both L0 backing pages remain allocated; this is not a backing retirement.

The decoded native INVEPT uses a fully read m128 operand in private guest RAM,
full 64-bit type, and an explicit WB/four-level/AD0/type1 capability validator.
The current root is reset only in the executing owner's sparse tree, including
actual local INVEPT, then its complete domain/incarnation/CPU/sequence ticket
is committed. Only afterward does L0 return VMsucceed. Other legal uncached
roots, including zero, require no backing lookup and are local no-ops.
Unadvertised type/EPTP capabilities yield error28 (or invalid without current
VMCS); nonzero descriptor high-qword remains a conservative unsupported stop.
Malformed caller width is not mislabeled as a guest instruction failure.

While AP is stopped, BSP verifies AP's generation, fill count, live pool count
and f000 cached HPA metadata unchanged by BSP's local instruction. Only then
does AP return to its L1, which executes its own native INVEPT before reading
the new mapping. The test does NOT require hardware without an INVEPT to
retain and return an old cached value. Both second L2 reads must instead show
7788778877887788 after each owner's own invalidation.

A local instruction does not close/open the domain reclaim gate, broadcast to
another owner, authorize HPA reuse, or flush EPT01. Shared mapping ownership is
unchanged; all old backing is retained until final dual CLEAR/OFF/park. This
single-writer root-stopped interval avoids concurrent page-table stores/walks.
The phase callbacks are fixture synchronization only, not the INVEPT decoder
or architectural validity test. Single EPTP/type1/4K/WB/AD0 remains the scope.

## N34B hardware evidence (2026-11-09)

Kernel `0fcca294ec3723399d45493e54758299fe6ecff5dd4bdf0b8419466784849c3d`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs show native L1 INVEPT owner0 and owner1, two new-value reads
0x7788778877887788, and `N34B_PASS native_L1_INVEPT=2 local_owner_flush=2
shared_writer=1 remote_software_cache_preserved=1`, with no failure markers.
No QEMU remained. Evidence: `libfile_4090a0bcef388191a019b3ecd1718d9c`.
This proves local software-cache scope and each owner's fresh read after its
own flush, not that unflushed remote hardware must preserve an old translation.
Scope remains single EPTP/type1/WB/AD0, controlled writer with remote root
quiescence; descriptor alignment/single-page limitations fail closed.

N34B final independent full regression: `efi/build/nested34b-review-full.log`,
`FINAL_STATUS=0`, 66 marked tests plus console-memory; final kernel unchanged.

## N34C1: per-owner two-root registry, no eviction

Each owner has a separate dynamic registry using only its private preallocated
96-page pool. Registry budget is two resident contexts with eviction disabled;
this budget does not promise arbitrary workloads will progress. All metadata,
tree nodes and leaves consume the pool. Shared immutable EPT12 roots A=1001e
and B=1401e map the same f000 GPA to different retained WB pages. All eight
EPT12 table pages are read-only in both EPT01 and both virtual EPT12 mappings.
No shared mutable registry, typed lease, A/D or table writer is introduced.

Native L1 VMWRITE changes its EPTP field. Before a virtual L2 entry the owner
selects the complete key, pins the new item, writes its physical EPTP into
VMCS02, and only on success unpins the prior referenced item. Selecting the
same item temporarily holds two references then returns to one. Uncertain
VMWRITE leaves pins retained until CLEAR; internal sparse-miss resume uses the
same already-pinned physical VMCS/tree. Destruction releases pins only after
actual VMCS02 CLEAR, invalidates locally, returns loans, and finally BSP frees
the original pool only after dual OFF/park.

Both owners really read A/B/A/B/A. After B is warm, each executes type1(A);
after returning to A each executes type2(all local contexts). Type2 is
advertised only in this profile and deliberately receives garbage low64,
which is ignored. Descriptor high64 remains a policy refusal. At each
invalidation barrier the AP has actually exited and waits in root; BSP
verifies both AP cache generations, fill counts, owned-path lookup/HPA metadata
and pool live count unchanged by BSP's operation before releasing AP to do
its own. No requirement is made that hardware must retain old remote entries.

Persistent tests combine private pool with registry budget exhaustion, pin
switching, root-specific/full reset, failed flush poison and retained loans,
then safe destroy retry. This milestone does not exercise eviction; that
requires a distinct low-budget multi-root workload and additional hardware
evidence. It is not a general concurrent L1 page-table writer implementation.

## N34C1 hardware evidence (2026-11-09)

Kernel `902cbef47d25e3195d7cf2d021def5759875aa4d4b225c7801398530f51ef2be`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit33.
Both logs show `N34C1_PASS owner_registries=2 roots_per_owner=2
native_type1_type2=2x2 reads_ABABA=2 no_eviction=1`, without error markers.
No QEMU remained. Evidence: `libfile_efa540c3c7588191b68ecf0cff512f3a`.
This validates per-owner two-root switching and local type1/type2 operations;
context eviction is deliberately excluded from this hardware milestone.

N34C1 final independent full regression: `efi/build/nested34c-review-full.log`,
`FINAL_STATUS=0`, 67 marked tests plus console-memory; final kernel unchanged.

## N34C2: owner-local eviction and same-key reconstruction

Each owner retains the two-context residency budget but enables the existing
dynamic registry's inactive, unpinned eviction. A third immutable EPT12 root C
at 18000..1b000 maps f000 to retained WB data page1c000. All twelve table pages
are read-only for L1 and L2; CPU1 private pages begin at21000 and do not overlap.

The native sequence is A/B/C/A/A/C. Selecting C drives A out while B is the
VMCS-pinned item. Returning to A constructs a new sparse context and evicts B
while C remains pinned. The rebuilt A has real misses before its read. The
fourth exit performs local type1(A), then another A read; the fifth performs
local type2 before C. Remote owner A/C snapshots are taken only while that
owner is stopped after its actual exit. Root HPA reuse is allowed; successful
fresh-root initialization includes local invalidation. Evidence associates
actual fill key and owner trace, four creations and two evictions per owner,
rather than assuming a newly allocated HPA must differ.

The owner pool remains 96 pages and may safely run out; it is not unbounded
PMM fallback. Candidate metadata/root are allocated and initialized before
victim selection/commit, so peak ownership exceeds the resident count. Pure
tests inject all three empty-candidate allocation failures at capacities6/7/8,
preserving active/pins/live; protect an inactive-but-pinned item; force victim
flush failure retaining candidate plus victim under poison; and retry destroy
only after references are cleared. These measured pure workloads peak at9
pages empty, or16 with one mapped leaf, not a universal worst-case estimate.
Runtime logs actual peak pool use. Shared backing never becomes reclaimable
merely because a local context was evicted; all remains until dual clear/OFF.

## N34C2 hardware evidence (2026-11-09)

Frozen kernel `7c6e15a4ea6480072c2c88ccd29fa4aad23f7ac501c54faf8008eb388b64d035`
passed isolated archbook KVM `-cpu host -smp 2` at 256/512 MiB, exit33.
Both logs contain `N34C2_PASS owner_registries=2 budget=2 roots=3
recreated_A=2 evictions=2x2 type1_type2_local=1`, actual A/B/C/A/A/C
values on each owner, four creations and two evictions each, and no error
markers. Each pool peaked at 37 pages in the 256 MiB workload (capacity96).
No QEMU remained. Evidence: `libfile_626180c0c7bc8191892d6ed2f112ab80`.
This does not extend immutable WB/AD0 profile to arbitrary concurrent writers.

N34C2 final independent full regression: `efi/build/nested34c2-review-full.log`,
`FINAL_STATUS=0`, 68 marked tests plus console-memory, frozen kernel unchanged.
