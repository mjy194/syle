# Firmware HLT idle and interrupt wakeup

The 2026-11-14 isolated native probe passed three complete wait/wake/EOI cycles
beyond the former HLT stop at `0x70d09e0`. The guest returned to its idle loop
after each interrupt. The probe stopped intentionally after the fourth HLT;
interactive UEFI Shell and firmware-launched Linux remain separate acceptance.

## Completion and waiting contract

The opt-in `firmware-halt` target adds `firmware_halt.c` and a shared observer
adapter. Ordinary quiet and CODE/PAUSE targets do not enable HLT handling.
The direct-Linux `guest_wait.c` path is unchanged.

The handler admits the existing CPL0 long-mode firmware profile only:
actual owned `F4`, raw exit reason12, length1, HLT exiting, secondary controls
`0xc2`, active guest state, SS DPL0, IF1, and clean debug/event state. It checks
`IA32_VMX_MISC[6]` and the existing preemption/host-event exit controls. It
rejects IF0 and unsupported modes/events rather than inventing a wake source.
Successful completion clears RF and any STI/MOV-SS shadow, advances RIP once,
and sets VMCS activity to HLT. Partial or reentrant writes permanently stop
the transaction; every rejection also stops the outer observer.

A separate software waiting flag survives VMX-preemption exits. With no
injected interrupt, only reason52 at the unchanged post-HLT RIP/flags is
accepted. If hardware reports active state, the handler restores HLT before
another entry. No instruction byte is retired again. With a staged vector32
interrupt, the existing `ftid_after` identity/entry/receipt checks must confirm
delivery, and VMCS activity must be active. Only then is the software wait
released. No software write forces active state and no extra IRQ is created.

This follows Intel's distinction between VMX timer exits, guest activity and
VM-entry event delivery; see SDM sections 26.2 and 27.7.2–27.7.4 in
[Volume 3C](https://cdrdv2-public.intel.com/812396/326019-sdm-vol-3c.pdf), and
the HLT capability in Appendix A.6 of
[Volume 3D](https://cdrdv2-public.intel.com/843859/332831-sdm-vol-3d-dec-24.pdf).
Clock samples, timer ownership, APIC priority/IF gating, IRQ receipts and EOI
handling continue through the existing paths. GPRs, memory, timer values and
intercept controls are not modified by the HLT completion module.

## Native result

The runtime was copied from the verified final PM-countdown source, including
its PM1, DE, PCI, fw_cfg and compact-trace overlays. The checkout observer is
older; its successful build is not substituted for this native evidence.

| Evidence | Result |
| --- | --- |
| First HLT | `F4`, RIP `0x70d09e0`, flags `0x202`, blocking0, pending0, ISR0 |
| Completed HLT instructions | 4; each advances to `0x70d09e1` and waits |
| Uninjected timer exits | 53; RIP remains `0x70d09e1`, logically halted |
| Verified interrupt wakes | 3; distinct receipt attempts54,55,56 |
| First exits after wake | reason28 at `0x70d1fa6`, inside firmware interrupt code |
| Guest EOI | 3 committed APIC `0xfee000b0` writes at RIP `0x70ad7f1` |
| HLT observation interval | 0.3005169 s across three 100 ms timer periods |
| Completed dispatches | 86,510 / 131,072 |
| Reference elapsed | 19.525514 s / 30 s |
| Compact trace | 1,849,729 / 2,097,152 bytes |
| Trace integrity | failed/incomplete/dropped all zero; 4,638,148 selected logical bytes |
| Positive outer process | Exit35, 160.213 s, no timeout, cleanup complete |

On this KVM run, all 53 uninjected timer exits saved activity1. The activity0
re-halt branch is covered by the host model, not claimed as observed natively.
Three distinct receipt/EOI/next-HLT sequences establish return to the idle loop;
a receipt alone would not prove that the guest handler executed or returned.

The unchanged 131072/30 s/2 MiB/per-entry budgets were sufficient. The separate
probe condition stopped after four HLT completions, an uninjected idle slice,
three verified wakes and three guest EOIs. The ordinary observer still returns
its diagnostic exit35 and prints a generic `failed reason` footer because it
never claims full boot acceptance. The `OVMF.HLT probe_complete` record is the
successful bounded-test result; the final reason12 was handled, not rejected.

## Verification and artifacts

- Owned-memory HLT suite: ASan/UBSan and freestanding build passed. It covers
  VMREAD failures, instruction/mode/control rejections, IF0, RF and interrupt
  shadows, 4 KiB/2 MiB fetches and a one-byte page-end instruction, uncertain
  writes, reentry, unchanged backing, repeated timer re-halts, and integration
  with the real timer IRQ session/receipt/EOI model.
- Existing IRQ-window, IRQ-delivery and preemption-resume suites passed.
- Both checkout and frozen-runtime builds passed. The ordinary quiet observer
  was separately compiled and contains no HLT module/adapter symbols.
- Original production ELF parser/load ASan/UBSan checks passed: file5,163,528
  bytes under 5 MiB, loaded span7,684,096 under 8 MiB; no undefined ELF symbols.
- Fresh tiny and no-Hyper-V controls exited33/35 at their expected markers,
  neither reaching WBINVD. The positive run passed the existing QMP device
  graph/module admission. No QMP socket or candidate QEMU process remained.
- The ordered-log analyzer verified four HLTs, 53 idle slices, distinct IRQ
  receipts, guest EOIs, terminal counters and trace integrity. Six altered logs
  (false wake, advanced idle RIP, missing receipt/EOI/footer/completion) were
  rejected.

One positive attempt recorded one architectural L1 WBINVD, moving the ledger
44→45; this is not a physical-flush count. Host settings were unchanged.

Evidence: `efi/build/firmware-halt-20261114/summary.json`,
`native/halt-analysis.json`, raw serial/QMP records, source/media manifests,
loader/build logs and `evidence.tar.gz` in the same directory.

Native kernel SHA256:
`505334378b78179f6feab72451985be9c1a916d27b28e1c547edd1a8a39dd818`.
Raw serial SHA256:
`089ed0dee8106c4aa9a2b9d907072b741616ed0aa0882295f720f6595731bd6f`.

Reproduction commands are in
`experiments/m2-snapshots/halt-progress/README.md`. The next M2 work is console
input/output and firmware-to-Linux acceptance; this probe supplied no input
and did not test storage or a Linux boot.
