# Enumeration-only noDMA v2: actual bounded result

Independent raw review 2026-11-13. Archive Library
`libfile_fde3_f-4e59db6f2e0c819187e7b060bfe8a3db_ab3ac89c6f9c81918c711418a26b62e9`,
SHA256 `3f3f8870701168d18be774a8957d863cc3524de06efcd978d95dbf84e8461e57`,
6,625,676 bytes. This is the run25-based v2 candidate, not the later rebase.

Raw host-tests.log records all seven fresh GCC16 host processes PASS. Disassembly
contains three refuse_admit/read/write bodies, each mov eax,0;ret, and actual
observer calls to vfb_init_nodma_probe and vfb_io. Tiny exits 33; no-Hv exits 35.
Positive result records admission_graph_equal=true, continued_once=true,
returncode=35 and elapsed=18.917002383997897. Serial confirms completed_dispatches
0x800 and observation_exit_budget=2048, last exit reason30, qualification0x700000,
RIP0x7acd797 after RTC index D OUT70. Trace requested/buffered/sink bytes all
0xb2f0a, dropped=0, incomplete=0. STOPPED cleanup marker exists; residual-process
file is empty. No IRQ receipt was reported. These facts do not imply timer or
IRQ correctness for all paths.

**INCONCLUSIVE for block discovery:** no confirmed block enumeration, no block
DMA/data transfer, no filesystem or Linux shell. It is not evidence that a
functional disk failed. The inherited exit bound stopped observation before the
run26 hostbridge BAR0 access (dispatch2118). Do not repeat this candidate blindly.

Next isolated candidate rebases the same noDMA modules onto the exact actual
run26 source archive (`7d60c4fe7613f0b078296524bd3d7168c5088b0df9175a3902c2011d4cb07204`),
which already contains the 3072 observation cap and RTC diagnostics. This is a
new reviewed experiment, not an in-run budget extension. It excludes the parallel
run27 no-BAR macro object, since its own projection handles no-BAR configuration.
Native DMA refusal stays in place. No default checkout modification or VM launch
is authorized by this document.

## Reviewed run26 rebase handoff

Independent review verified fresh base/overlay, seven host processes, ten source
files unchanged from v2, three refusal bodies and final observer calls/no vfr.
Frozen archive Library
`libfile_fde3_f-ee16ac3124888191a010444df19dec5a_ab3ac89c6f9c81918c711418a26b62e9`,
SHA256 `407ba8521cd78662fb7960f9b388643d3cdd456539962cac6e1e394c960d747c`,
4,078,772 bytes. CONTROL SHA256
`8e336b48860df8b1dfd2eab3528397b6b3f3eaed405ce56b744302a8f51552b1`.
Runbook: `pci-nodma-run26-package/RUNBOOK.md`. Handoff is for a fresh GCC16 gate,
static checks and root-authorized single serialized diagnostic, not a result.

This is NOT a one-variable BAR fix versus the ordinary firmware branch. The
projected profile has 256-byte config reads, synthetic PIIX function identities,
no-BAR zero reads/probes, selected RO/RW/zero-status write semantics, live PM
state, an extra block BDF and its restricted D000 aperture. Its source does not
alter clock/IRQ logic, but device behavior and timing may change. Any observed
progress cannot be attributed only to the budget or equated with run27 no-BAR.
Source hashes/fresh compiler gates and raw observations must distinguish these
profiles. DMA remains refused, even if configuration enumeration succeeds.

## Actual run26 rebase: PCI identification observed, allocation not observed

Raw Library `libfile_fde3_f-51b38623019c8191a94c469a142aef12_ab3ac89c6f9c81918c711418a26b62e9`,
SHA256 `06a73bc2a437955a4c28985e1958190a8c0207ec99614ba1f3ee77377774eda4`,
6,663,934 bytes. Independent archive hash and complete logged-cycle parsing via
`verify_nodma_config_trace.py`; output `evidence/nodma-run26-config-replay.json`.

1509 delegated PCI/board cycles, 310 enabled CFC configuration cycles, zero
rejected cycles. BDF 00:02.0 has 17 accepted dword reads: offset0 twice returned
0x10011af4 (device1001/vendor1af4), then every dword of 0x04..0x3c. Class/revision
at8 returned0x01000000; BAR0 at0x10 returned1 (unassigned I/O BAR). There are NO
logged writes to this BDF and NO D000-DFFF aperture cycles. This is positive
proof that this firmware run probed the advertised block PCI identity/config
header. It is NOT complete resource assignment/enumeration or driver binding.

The final cycle is accepted CF8 dword input with latch0, after PIIX PM BAR0x24
probe/restore. Serial reports exactly3072 dispatch budget, RIP0x7089947,
qualification0xcf8000b; trace requested/buffered/sink all0x180bef and zero dropped.
Inputs reported fresh GCC16 seven host checks/build/static refusals PASS, a
single29.13s positive, exit35/cleanup/no residual. No disk DMA was enabled.

Next diagnostic should target continuation through resource assignment and
first attempted virtio initialization, retaining fail-closed PFN admission.
Do not increase this completed run or retry it. Before a new bounded experiment,
review expected BAR sizing/assignment sequence against the current D000-only
profile, preserve full single-CF8 and live PM semantics, and select a new finite
exit/trace budget with root. A likely first config write outside the supported
BAR aperture is a useful precise compatibility stop, not a reason to silently
accept arbitrary mappings. Actual DMA/backing/IRQ/ownership remain separate
unmet requirements. Any longer observation is a new candidate and must pass
static/host review and serialized controls, not an implicit continuation.

## Separate 4096 candidate handoff (not executed by author)

Root requested a separate bounded observation, independently reviewed with
fresh seven host tests plus actual trace full-capacity/overflow test (8 processes).
Compared with the previous3072 overlay only four numeric text occurrences change
(loop bound, final comparison and two log strings). Full projection and noDMA
modules unchanged;2MiB trace,30s reference, per-entry VMX slice and outer180s
remain. Trace failed gate is present both before loop work and immediately before
lvs_run. Prior observed trace extrapolates slightly above2MiB at4096, so capacity
may stop first; incomplete evidence is inconclusive, never a reason to expand,
resume, drop logs or retry. This is not a guarantee of resource enumeration.

Frozen Library
`libfile_fde3_f-c42839692f448191bef1cdfd87e9c25b_ab3ac89c6f9c81918c711418a26b62e9`,
SHA256 `cd9626443e04d9292d87cb3445c5fc2d55e59910f01bab08c8be495ce2d5885b`,
4,080,840 bytes; CONTROL
`6525903415d4add10268116a6c4fe4d1985307d5269e6edd331f738987203a87`.
Runbook `pci-nodma-4096-package/RUNBOOK.md`. Inputs must freshly run GCC16 gates,
including trace-bound-check.sh, static/loader/media/identity checks and serialize
ONLY after mainline round30 cleanup. Native DMA remains refused.

## Actual4096 attempt: overflow, retained prefix only

Library `libfile_fde3_f-5dc13651450c819199431d243c2492ee_ab3ac89c6f9c81918c711418a26b62e9`,
archive SHA256 `b82438ecf99dd154ddd17573cb8b6ecc82f83e3047f6a17055cf07df334a6f48`,
6,674,710 bytes. Independently downloaded/hash verified; serial SHA256
`3ac41fca3500fd51e9d171268b42e37fc61279737e59820f1c60286ce3d53501`.
This NEW run ended at3705 dispatches on trace overflow, NOT the4096 exit bound.
failed=incomplete=1; requested0x2007d7, buffered0x1ffff0, dropped0x7e7. The retained
prefix is incomplete for overall execution. Inputs reports one34.73s Hv run,
exit35, cleanup/no residual, no retry. No attempt to expand trace or resume.

The2143 fully logged cycles in the retained prefix include469 enabled config
cycles and79 disk-BDF cycles,14 writes. Disk BAR0 sequence is initial1, write
FFFFFFFF, readFFFFFFC1, restore1: actual64-byte I/O BAR sizing was observed.
Other five BAR probes return0 and restore0; ROM FFFFFFFE probe returns0; command
word0 write accepted. No assigned nonzero BAR address or D000-window access
appears IN THE RETAINED PREFIX. This is not a claim about all dropped operations.
Last complete cycle reads absent function80003800→FFFFFFFF accepted1; later
terminal diagnostic RIP/qualification need not describe that last retained event.

Structured retained-prefix evidence: `evidence/nodma-4096-prefix-replay.json`.
Do not call this complete enumeration, driver initialization or block transfer.
Overall outcome remains INCONCLUSIVE. The next candidate must solve evidence
capacity first. OVMF branch owns an independent lossless compact-trace proposal;
replay/roundtrip and sticky-overflow tests precede any noDMA adoption. No automatic
retry, raw-stream omission or larger memory window is authorized here.

## Compact6144 handoff, after mainline native observation

Root reports mainline compact native observation reached3795 exits/new fw_cfg E
stop with encoded871820/decoded2173558 and failed/incomplete/drop0. The encoded
buffer was not exported, so this is counter/sink consistency, NOT independent
native binary roundtrip. Do not transfer mainline device-progress claims to the
noDMA profile. Independent noDMA integration review fresh nine host processes
PASS, exact compact module equality/staticfoc+vfb/no fot/vfr/undef confirmed.

Frozen noDMA compact6144 Library
`libfile_fde3_f-3e6c4941a6908191a5526c47868903fe_ab3ac89c6f9c81918c711418a26b62e9`,
SHA256 `bfecd40246a50771679c793778e02fd3727bfbedd93c9af9ee18052e0451baf0`,
4,095,955B; CONTROL
`c4c6509d599503a68499427f2e0663bdbfab02da0ceb971ec378bcb59d579f84`.
Runbook `pci-nodma-compact-package/RUNBOOK.md`. Separate freshGCC16 host/build/
loader/controls precede root-authorized serialized one-positive experiment.
6144 total exits does not remove the separate4096 delegated-board-cycle cap.
2MiB/30s/outer180s remain; overflow/unknown completion never retried. The
host-replayed old prefix proves reconstructed bytes under resegmented events,
not native event boundaries or timing. Native DMA still refused.

## Compact6144 v2 native observation (2026-11-13)

Archive Library `libfile_fde3_f-1560e9eea5f081919ff95b25ec463110_ab3ac89c6f9c81918c711418a26b62e9`,
SHA256 `46691e488dcdd501721903bb1f834d1d5135ad3f54e2b4e0bd216befd7b6ae85`.
Serial SHA256 `23b151b273f33fe23fd774db74b7fe0c2ae7d2dd40ab7c34e25dec1a2c20d45b`.
The complete decoded trace has 4096 pci_cycle records after CR normalization;
the runner's summary count 3824 does not match this full parser and must not
be used as the coverage count. All 4096 records were accepted. There were 957
enabled CFC configuration cycles, including 146 for BDF 00:02.0, with 28 writes.

Disk BAR0 sizing sequence occurred twice: read 1, write FFFFFFFF, read FFFFFFC1,
restore 1. Both complete rounds therefore detected a 64-byte I/O BAR but did
not assign an address. All disk command writes were zero. The complete trace
contains no D000-DFFF access, feature negotiation, PFN setup or notify.
This does not establish complete resource allocation or device initialization.

Execution stopped at the separate 4096 delegated-board-cycle gate on dispatch
5697, not the 6144 total-exit cap. The last accepted disk configuration access
was offset 0Ch dword read; the next board access was refused by the budget gate.
Trace failed/incomplete/drop all zero, decoded/requested/accepted/sink 3743318,
encoded 1494785. The encoded buffer was not exported: this is decoded counters
and raw-stream consistency, not independent native binary roundtrip proof.
Inputs reports exit35 cleanup/no residual and one WBINVD, ledger34->35.

Offline parse: `evidence/nodma-compact-v2-complete-replay.json`, produced by
`verify_nodma_config_trace.py` from raw serial. No VM was rerun for this review.
