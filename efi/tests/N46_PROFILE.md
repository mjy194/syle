# N46 bounded fixed UC AD1 acceptance profile

This profile is software-reviewed. Physical guest acceptance and complete
regression are parent-owned gates, not implied by any host test or build.

- Build: `make -C efi smp-nested-atomic-uc` after sourcing cloud-env.sh.
- Combined focused software gate: `bash efi/tests/nested46.sh --host`.
- `--runtime` only proves the explicit TCG CPUID_VMX0 skip; it is not KVM/UC.
- The exact final ELF must equal the ESP kernel byte for byte. Fresh builds can
  differ in debug paths; hash the exact image actually used by each gate.
- Required supported scope: two CPU owners; one retained UC 4KiB data page at
  L1 GPA f000; IPAT1/AD1 EPT12 and private EPT02; WB source/private tables and
  alternate source-C data. Physical fixed EPT roots remain AD0. No live retype,
  dynamic source structure, reverse WB transition, reclamation or huge UC leaf.

A physical acceptance log must show, in order:

1. Both owner snapshots with matching CR3/PAT and the exact target HPA/host PTE.
   Initial host WB and intended UC words differ only by PAT3 encoding 0x18.
2. Forty completed provisioning receipts with exact ordinal, phase, CPU and
   fixed EPTP attribution. Retention is first and source publication is last.
   INVEPT_BEFORE has ordinals 5–14, INVEPT_AFTER 28–37: the same five distinct
   physical EPTPs, low bits 0x1e, once on each CPU per phase. Host INVLPG is
   separately attributed at 16,17,21,22 and WBINVD at 18,19,23,24.
3. N46B_BOOTSTRAP_COMMITTED before any guest checkpoint. This does not substitute
   for private-root initial INVEPT or later reset/retire receipts.
4. Thirteen checkpoints per owner. Source keys 0x1005e and 0x1405e use UC/IPAT1
   private leaves; 0x1805e uses WB/IPAT1 alternate backing. Private EPTP low bits
   are 0x5e. Both owners independently trap/promote their first writes even when
   shared source D is already set by a peer. Read tracking remains RO/D0.
5. Two complete reset rounds with eight exact reset receipts, four pinned
   source roots per round, unchanged launch state and original-RIP retry,
   retained data, unchanged type epoch 1 and A/D epochs 1,2,3. The source graph
   is nine WB pages, with A/D writes only after publication.
6. Five terminal root retirements, source pins drained, actual local VMCS clear,
   both VMXOFF, AP park and backing retained. No source/data free or reverse
   type transition is claimed. The final SMP.N46B_PASS is necessary but does not
   replace checking the detailed receipts and assertions.

Full host alias/ownership audits, protected-window nonaccess, dispatch fault
latching and source graph checks are assertion-backed behavior. Serial counters
alone do not independently establish those internal facts. Preserve that
boundary in any acceptance report. In particular, physical UC execution is not
proof of a naturally occurring hardware CAS collision; host-thread CAS tests
are separate software evidence.
