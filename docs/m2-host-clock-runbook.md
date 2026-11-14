# Resume-only runbook: experimental Hyper-V host clock

Status: frozen cloud candidate; **do not start archbook runs while the user's
Codex restart pause remains in effect**. No new native clock result exists.

Candidate: `host-clock-optin-v1.tar.gz`, SHA256
`64818433e263430f534974620a7e9ba89d7fd8a0c49cc26b2053f07220c82123`.
Library: `libfile_fde3_f-124426f74480819187e3b54cfb09ef1f_ab3ac89c6f9c81918c711418a26b62e9`.
Apply its 31 files to a new copy of the accepted clock-v3 baseline source.
Keep prior sources, binaries, ESP, VARS and all historical evidence untouched.
Compare any overlapping local changes before applying. It includes the slice
header/session diagnostics needed for the new compile target. Inputs remain the
same clock-v3 bzImage/initramfs/manifest; rebuild no guest fixture for this step.

Cloud validation: source/planner/domain/Hv admission tests, dispatch transaction
tests, default PIC04 integration and session regression passed. Independent
review passed. Kernel target linked. This does not authorize claiming runtime
accuracy or changing the production/default build.

Build:

```
sh krnl/tests/linux_host_clock.sh
sh krnl/tests/linux_host_clock_dispatch.sh
make -C efi -f Makefile -f managed-host-clock.mk managed-host-clock \
 MB_INPUTS=/absolute/clock-v3-inputs \
 MB_MANIFEST=/absolute/clock-v3-inputs/manifest.json
```

Pair `efi/build/krnl-managed-host-clock.elf` with the unchanged terminal manager
`efi/build/mgmt-guest-terminal.elf`, using the established independent ESP and
VARS runner. Capture actual compiler, binary hashes and full QEMU command.

After the pause is lifted, first confirm the installed QEMU recognizes the
requested CPU property without starting a parallel VM. Keep the established
q35/KVM, single vCPU, RAM, firmware, storage, no-network and console choices.
The only proposed positive-run CPU change is:

```
-cpu host,hv-time=on
```

This property is a candidate, not verified installed-host compatibility.
Do not add `hv-frequencies`, change host security or claim CPUID.15 has become
valid. If QEMU rejects the property, retain its error and stop this branch.
Before a positive run, a negative run with the prior `-cpu host` must print
`HOST_CLOCK Hyper-V reference counter admission rejected`, with no managed
guest launch. If it unexpectedly admits, inspect exact hypervisor features
rather than assuming the negative test passed. Use a short outer timeout for
negative admission because the normal kernel failure path may halt.

Positive-run prerequisite: log `HOST_CLOCK experimental Hv#1 reference-counter`
and the fixed ABI rate `hz=0x...989680` (10,000,000), stable vCPU identity and
max_gap=600,000,000 counter ticks (60 seconds). Treat an inaccessible reference
MSR as rejection via the armed native fault fixup, not a platform crash/retry.

Run the existing five-mode observer with a fresh output directory and one
repeat, bounded by an outer timeout; require Linux ready, exact markers and
checked management cleanup. Then plan fresh repeat/reversed-entry-bound tests,
10s/60s elapsed checks, busy vs HLT, explicit STOP/START, and rate error vs
interrupt-delivery latency. These longer scenarios are not all implemented by
the current five-mode probe. Do not pretend existing clock1000 covers them.
No host suspend or VM live migration is permitted in this first experiment.

Transaction caveat: before-entry synchronization commits time before later
VMCS preparation. Failure afterward is terminal/no-resume, not a rollback of
elapsed time. Dispatch emulation separately stages and atomically publishes
clock/device state on success. Reserved external input is busy/unaccepted;
STOP that cannot safely synchronize fails closed, then teardown can clean the
already-failed session. Preserve all failure logs, never mask them with restart.
