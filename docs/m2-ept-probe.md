# Opt-in managed EPT permission probe

`managed-ept-probe` exercises the **actual `linux_guest_memory` and managed
session/VMX ownership lifecycle**, not the older small EPT diagnostic guest.
It does not run a Linux workload: the normal fixed Linux/initramfs are loaded and
validated, then the first VM entry is redirected to a controlled long-mode
payload before Linux executes. Do not describe this as modifying live Linux or
as validating arbitrary runtime EPT permissions under Linux load.

```
make -C efi -f Makefile -f managed-ept-probe.mk managed-ept-probe \
 MG_INPUTS=/absolute/path/to/historical-fixed-inputs
```

Pair `krnl-managed-ept-probe.elf` and `mgmt-guest-ept-probe.elf` in a new isolated
ESP. Keep the existing fixed-input SHA gate; the probe never bypasses it. Use
single vCPU KVM, separate firmware VARS, no network and a bounded outer timeout.
The ordinary Linux and PF targets, firmware work, and default build are unchanged.

## Payload and checks

- Three pages at GPA `0xa000`, `0xb000`, `0xc000` hold code, data and execution
  target. Preparation verifies no overlap with kernel envelope/initramfs,
  bootstrap page tables/GDT/TSS, stack, parameters or command line.
- The actual bootstrap guest page tables identity-map these pages. The actual
  EPT leaf points to the corresponding owned HPA. Selected-page ownership and
  table aliases are checked before the pre-entry payload is written.
- The original Linux VMCS plan remains unchanged. A one-time `before_entry`
  changes RIP to `0xa000` and performs INVEPT before guest entry.
- Read fault: data leaf R/W/X = 0. This avoids both invalid W-without-R and a
  dependency on execute-only EPT support.
- Write fault: data leaf R/X, W denied. Execution fault: target leaf R/W, X denied.
- Each exit must be reason 48 with exact known RIP, GPA, GLA and access/effective
  EPT permission bits; no vectoring or pending injection is allowed. Optional
  qualification bits 9–11 do not authorize a repair: identity/owned mapping and
  exact access checks remain mandatory.
- Restore only the saved original leaf, execute INVEPT and **do not advance
  RIP**. A later exact VMCALL checkpoint validates the read value, actual stored
  value, or execution marker. Only then is the next fault armed.
- Three faults and three checkpoints are required. Repeated fault without its
  checkpoint, unexpected exit/fields, failed reads/writes or failed INVEPT latch
  failure and forbid another entry. Normal session destroy retains its existing
  VMXOFF/invalidation/ownership cleanup obligations.
- Preemption-timer exits preserve state and do not count as a successful
  permission test; the probe has a hard total exit budget.

The console reports each accepted phase's RIP/qualification/GPA and final kernel
completion. The Ring3 manager requires the completion byte, then successful STOP
and DESTROY before printing its success marker and exiting 0.

## Host checks and hardware acceptance

```
sh krnl/tests/linux_ept_probe.sh
```

The host test uses actual `lgm_prepare` output and mocked VMCS/INVEPT operations.
It covers R/W/X/checkpoint ordering, no RIP write on repair, exact matching,
duplicate rejection, partial failure, overlap and invalid guest page table
rejection. It is not VMX execution.

Hardware acceptance requires captured three reason-48 phase records, final value
checkpoint completion, Ring3 success marker and `MGMT.EXIT=0`, plus no residual
VM after the test runner cleans up. No such result is implied merely by a build
or passing host test. See `m2-ept-contract.md` for the broader policy/remaining
Linux isolation boundary.
