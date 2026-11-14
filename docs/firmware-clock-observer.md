# Explicit nondefault OVMF clock observer

Base: native-verified idle-LVT candidate. New target `firmware-ovmf-clock` produces `efi/build/krnl-ovmf-clock.elf`, not the register-only ELF. Outer QEMU must explicitly enable the tested Hv time interface (`-cpu host,hv-time=on`); plain `-cpu host` is the required source-admission negative control. This is not a bare-metal clock solution.

The observer selects a 100MHz virtual APIC bus driven by Hv's specified 10MHz reference counter. It samples on entry, exit and MMIO, runs at most 128 exits with 2^24 execution-budget slices (rejects unsupported shift), and stops on every unmasked expiry without consuming it. There is no IRQ injection. Guest instructions may pass an ideal timer deadline before the next exit; wall-time lateness is not bounded, particularly across outer scheduling pauses. Resumption observes elapsed time first; unknown, reversed, changed-CPU/epoch or excessively gapped samples terminate the run. Pauses are not subtracted or hidden.

A timer-MMIO transaction first persistently samples real state under exclusive ownership, then stages configuration. GPR/RIP/device configuration publish only after all hardware writes succeed. Any partial VMCS write is fatal; terminal ownership is never rolled back. The old source-free APIC model continues to own non-timer registers, while all four timer registers route to exactly one new counter model. Disabling software APIC while this timer runs stops rather than silently discarding events.

Build (supply local EFI and pinned firmware paths):

```
make -C efi -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk -f firmware-ovmf-clock.mk firmware-ovmf-clock EFI_PREFIX=... OVMF_CODE=... OVMF_VARS=...
```

Run host scripts `firmware_timer_100m.sh`, `firmware_timer_100m_mmio.sh`, `firmware_clock_apic_transaction.sh`. Check `nm -u` is empty and loader ELF bounds. Then serially run tiny regression, no-Hv negative control, and Hv-time observer in fresh isolated ESP/outer VARS, single-vCPU/no-network/bounded timeout. Preserve raw serial logs and confirm no residual VM. This is an observation run, always reports STOPPED rather than firmware boot success.

Overlay contains new target files and exact source/header dependencies for the existing Hv admission implementation. Do not silently overwrite a differing existing dependency: compare manifest and resolve first. No changes to generic native source are required; the new target uses an independent CPL0/IF0/no-armed/hook-checked read-only RDMSR wrapper through existing fault-fixup assembly.
