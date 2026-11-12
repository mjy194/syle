
## N31A: cache-maintenance rendezvous, no type transition

The two-CPU running-guest kick path now has an independent maintenance target.
After the AP's actual reason1/vector41 exit/EOI and quiescence, the existing
shootdown entry gate remains closed. Each CPU independently captures CR3/PAT,
checks CD/NW/PCID/PGE/LA57, proves the dedicated data page has MTRR WB, and audits
its full host root for a unique low 4K RW/NX WB identity alias. Full PAT and root
must match between CPUs, but unrelated CR/MTRR bits need not be identical.
The target is allocator-owned data page8, not mailbox/code/stack/paging storage.

A separate BSP-serialized epoch/phase model advances PRECHECK→TLB→CACHE→COMPLETE.
ACKs identify the actual CPU, epoch and phase and require the complete fixed
participant mask. AP vector40 only queues/EOIs; its main loop executes local
INVLPG and WBINVD with IF0 before release-publishing the phase result. BSP
acquires the matching result before recording that CPU's ACK. Duplicate/stale/
wrong-phase/wrong-CPU ACKs do not advance, and failure/timeout poisons the domain.
No stage opens guest entry; explicit final finish and the separate shootdown
barrier are both required before resumption. An unchanged-EPT per-CPU INVEPT
barrier follows maintenance; both CPUs then resume the original WB value.

This diagnostic changes no host PTE, EPT leaf, PAT or MTRR and is not a completed
multiprocessor memory-type transition. The single-CPU registered-lease gate is
unchanged. Failure retains resources; success still requires per-CPU VMCS CLEAR,
VMXOFF and AP park before freeing backing. Tests cover preflight resource/OOM
refusal, all pending-phase timeout/poison, epoch overflow and ACK negative cases.
Run `efi/tests/smp-maintenance.sh`; actual two-CPU KVM is separate validation.

## N31B: two-CPU WB→UC→WB private-page transition

This opt-in direct-guest fixture builds on N31A, without widening the registered
single-CPU lease service. It owns one shared four-level EPTP, a unique low 4K
host identity mapping and one private data page, never mailbox/code/stack/table
storage. Both CPUs first warm this backing, the running AP is kicked and both
entry gates close. Each CPU independently proves the same host CR3/PAT and
MTRR-WB target contract. PAT3 is UC on both CPUs; no PAT/MTRR is changed.

Each conversion has a separate monotonic epoch with six all-participant phases:
1. Audit all four owned EPT tables and all 512 PT leaves, snapshot the complete
   shape, detach every target alias, then actual local INVEPT on both CPUs.
2. Verify host PTE identity (only A/D may differ), publish nonpresent, then
   INVLPG on both CPUs.
3. WBINVD on both CPUs with the target absent from host and EPT mappings.
4. Verify host PTE is still zero, publish the new type, then both INVLPG.
5. Both WBINVD before any guest or explicit target access is allowed.
6. Recheck the complete detached EPT snapshot, publish all target leaves with
   the new type, then both INVEPT. UC uses IPAT1; restored WB uses original flags.

Per-phase publication, CPU ACK and advancement are separate operations. AP main
performs the privileged work after ISR EOI and returns epoch+phase only after
success. BSP keeps the existing shootdown gate closed across all six phases;
even after the transition model's final completion an extra acknowledged EPT
barrier precedes entry. Both CPUs read the UC data, exit again, repeat the full
reverse protocol to WB, and read again before VMCS CLEAR/VMXOFF/AP park/free.
Physical guest CR0 masks guard CD/NW; unknown MSR/CR operations fail closed.

No code dereferences target contents while the host PTE is nonpresent. Local
maintenance checks CPU state only, not an obsolete present-PTE assumption.
Timeout, mismatched phase or uncertain hardware result permanently closes
entry and retains all resources, without speculative rollback. Pure tests cover
missing/duplicate/stale/foreign ACKs, publication ordering, overflow, preflight
resource refusal, all-stage poison, multiple aliases and table tampering.
This is bounded two-CPU direct-guest UC support, not nested-context enumeration,
arbitrary EPT shape, multi-EPTP or WT/WC/WP multiprocessor transitions.
Run `efi/tests/smp-cache-transition.sh`; two-CPU KVM is separate evidence.
