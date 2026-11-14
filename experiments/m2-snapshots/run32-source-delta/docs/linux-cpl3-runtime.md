# CPL3 runtime and first-shell checkpoint

This increment is host-tested preparation, not Linux shell acceptance.
It does not relax EPT ownership, page-fault validation, event receipts, or
VM-entry failure handling.

## Legal CPL3 exits

The dispatcher now permits timer (52), interrupt window (7), CPUID (10),
and RDTSC (16), in addition to the existing strict exception path (0).
Other CPL3 exits, and all CPL1/2 exits, remain fail-stop. CPUID uses the same
selected virtual profile as CPL0. RDTSC checks guest CR4.TSD before changing
any state; TSD=1 remains unsupported/fail-stop rather than returning a value
that bypasses the guest's access policy. Timer/window exits never advance RIP.
Existing vectoring/event-owner and partial-write guards still apply.

The CPUID and RDTSC helpers accept CPL0/CPL3 only. The RDTSC caller owns the
CR4.TSD check. No new host hardware reads or guest capabilities are exposed.
This is not a general user-mode CPU emulator; PAUSE and privileged operations
remain outside the new CPL3 allowlist.

## Diagnose the observed error-6 stop

The earlier separate archbook runtime report for commit 2dc0b075 recorded
109076 exits, RIP 0x4a7839, CR2 0x549a50, PF error 6, after `Run /init as init
process`. That evidence supersedes the old candidate-only note for that
particular run, but does not prove successful PF injection or a shell.
A subsequent diagnostic KVM run reproduced this stop at dispatch stage 4,
with demand pending/count=1/7, entry-info=0xb0e, entry-error=0x14, length=0,
IDT-vectoring=0 and a new error-6 #PF. This identifies the old receipt gate,
not the new PF permission validator, as that stop's cause.

The follow-up adds a narrow consecutive-PF receipt path. It accepts only an
exact hardware #PF exit with supported error 0/2/4/6/20, zero IDT-vectoring,
entry-info exactly 0xb0e, zero entry length, and entry-error matching the last
owned PF history. It clears only the old software pending owner; the new fault
must still pass all existing owned-memory, permissions, metadata, repeat and
budget checks before reinjection. Timer/NMI/#DF/#TF and other exception receipts
remain rejected. Partial reads/writes fail-stop without resume.

Architectural basis: [Intel SDM Volume 3C](https://cdrdv2-public.intel.com/671506/326019-sdm-vol-3c.pdf),
sections 25.8.3, 27.6.1.2 and 28.2.4. Normal VM exits clear the entry-info valid
bit, so that bit alone does not prove delivery. A nested fault during injected
IDT delivery reports vectoring; direct double/triple faults are not the exact
#PF required here. A fault fetching the handler's first instruction is a new
fault after the original vectoring and still must pass the normal validator.
This is a bounded receipt policy, not general nested-exception handling.

`managed-pic04-progress` now emits a one-time failure snapshot even outside
its normal sampling interval. `dispatch_stage` records the last checkpoint:

- 0: reset/no dispatch
- 1: context/budget
- 2: VM-exit fields/CPL/RDTSC TSD
- 3: logical clock/device preparation
- 4: prior event receipt consumption
- 5: instruction/device route
- 6: exception/PF route
- 7: completed dispatch

The same snapshot includes early PF state and demand pending/count/failed.
The always-available PF diagnostic appends fields 28..33:
entry-info, entry-error, entry-length, IDT-vectoring, interruptibility,
activity. Existing indices 0..27 retain their meaning. All values are raw
post-failure observations, not proof that the VMCS or partial effects can be
safely resumed. The failure latch still prohibits resume.

## Reproduction

The opt-in output frontend is documented in `user/mgmt/CONTINUOUS.md`.
Keep the pinned kernel/initramfs and single-vCPU PIC04 profile. To diagnose
failures use `build/krnl-managed-pic04-progress.elf` with the continuous
management ELF, not the default lifecycle smoke.

Host checks include:

```
sh krnl/tests/linux_guest_dispatch.sh
sh krnl/tests/linux_cpu_profile.sh
sh krnl/tests/linux_clock_dispatch.sh
sh krnl/tests/linux_user_pf.sh
sh krnl/tests/linux_pf_diagnostic.sh
sh krnl/tests/linux_progress_diag.sh
sh user/mgmt/tests/guest_continuous.sh
```

The next runtime gate is to identify and pass the actual error-6 rejection,
reach `SYLE_LINUX_USERSPACE_READY`, and retain working timer/IRQ state.
The continuous frontend currently only drains output. A host-input source and
SEND/IRQ4/tty round trip are still required for interactive shell acceptance.
No KVM/VMX runtime execution of this increment has been performed here.

## User nonpresent reads

The second isolated runtime passed the old error-6 stop and reached a new
error-4 user read at RIP 0x479a5c / CR2 0x507240 after 109080 exits. The next
increment accepts nonpresent user reads while independently proving executable
owned user RIP. Present data mappings, supervisor ancestors, noncanonical
addresses, protection faults (5/7), invalid modes and duplicate/budget violations
remain rejected. Read data may have read-only or NX ancestors; those are not
permission failures for a read. No instruction is emulated and no page tables
are repaired. The receipt and entry-owner checks include exact error 4.

## Policy superseded for normal boot

Following the user's explicit direction, normal Linux guests now receive #PF
natively. The narrow reinjection machinery described above remains available
only in the explicit diagnostic build; it is not the default way to implement
Linux memory management. See `linux-native-pf.md` for the bit/mask/match policy,
CR2 handling, separate targets, and still-conservative vectoring/IRQ limits.
