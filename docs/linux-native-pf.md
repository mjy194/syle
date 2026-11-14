# Native guest page faults by default

The diagnostic profile intercepts all guest exceptions and only admits narrow
reinjection fixtures. That is not necessary for host isolation: private EPT
ownership constrains guest physical access. A guest page-table write-protection
fault (such as the observed error 7 at 0x53f560) should normally be handled by
the guest kernel, which knows its VMAs. The hardware evidence proves a write
protection fault; it does not by itself prove Linux's intended COW operation.

The default Linux guest VMCS clears **only bit 14** of the VMCS exception
bitmap. Page-fault error mask and match remain zero. All other exception bits,
EPT, CR controls, VM-entry validation and VMX ownership are unchanged. The explicit `MG_DIAGNOSTIC_GUEST_PF` build retains the historical all-bit
exception bitmap. The APIC activation gate checks the selected compile-time
bitmap rather than silently requiring the old diagnostic default. The named
`managed-native-pf` convenience target remains limited to PIC04.

[Intel SDM Vol 3C](https://cdrdv2-public.intel.com/671506/326019-sdm-vol-3c.pdf)
section 26.2 defines PF exits as the bitmap bit when error&mask equals match,
and its inverse otherwise. With mask=match=0 equality is always true; bit14=0
therefore means no ordinary guest PF exits, regardless of error code. This
does not disable EPT violations, which are a different exit mechanism.

Start a **fresh guest session**. Do not change a running VMCS profile or reuse
pending manual-PF state. The unchanged native assembly saves host CR2, loads
guest CR2 before entry, captures guest CR2 after exit, and restores host CR2.
Default startup does not grant the software PF injector its fixed-image permit;
pinned-input hashing remains mandatory regardless. A guest-native PF updates its own CR2 and invokes its own IDT handler. No host
page-table repair or faulting-instruction emulation is performed. An unexpected
intercepted PF in this profile fails closed rather than falling back to the
old diagnostic injector.

```
make -C efi -f Makefile -f managed-native-pf.mk \
  managed-native-pf MG_INPUTS=/path/to/pinned-inputs
```

Pair `krnl-managed-native-pf.elf` with `mgmt-guest-continuous.elf` in a fresh test
ESP. The target includes progress diagnostics. To additionally build the fixed
shell command probe, use `managed-native-pf-probe` and pair the same kernel
with `mgmt-guest-shell-probe.elf`; see `user/mgmt/SHELL_PROBE.md`.
No build command runs a VM or alters the current boot configuration.

`sh krnl/tests/linux_native_pf.sh` compares full default/native VMCS plans and
proves only exception bit14 changes, including mask/match behavior and
unchanged EPT/other controls. Existing native-context tests cover saved-state
contracts; a host-only test does not prove hardware exception behavior.

## Limits and acceptance

This is not a complete event-delivery redesign. The session still refuses
IDT-vectoring exits, and injected IRQ receipts retain their conservative
whitelist. An EPT exit or other intercepted event during native PF delivery may
still stop safely; do not remove those guards to make a test pass. Other guest
exceptions, including #GP/#DF, remain intercepted.

A runtime result must report whether it reaches the READY marker/prompt,
whether a command is accepted and answered, and whether the VM continues.
The fixed command probe is not a general interactive terminal.

## Explicit diagnostic build

Use `managed-diagnostic-pf.mk` and target `managed-diagnostic-pf` (output-only)
or `managed-diagnostic-pf-probe` (fixed command probe) to compile the old strict
PF-intercept path. These use separate `mdp-` objects. Do not reuse an already
running native session with the diagnostic profile or vice versa. The default
managed, PIC04 and APIC plans now use native PF, while all other exception bits
remain set. Tests compare both complete plans and keep the diagnostic PF helper
regressions independent from the normal runtime policy.
