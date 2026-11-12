# M2 Linux boot foundation and bounded probes

This is an intermediate implementation for the confirmed single-vCPU Linux
command-line goal, not milestone acceptance. Ring 3 lifecycle, firmware,
storage, display and interactive input remain separate requirements.

Implemented components: strict bzImage loading and boot parameters/E820;
private 128 MiB guest RAM/EPT with rollback; guest/host VMCS planning and
ownership cleanup; bounded native entry/exit state; CPUID, CR, EFER/GS handling;
page-table diagnostics and tightly constrained page-fault injection probes.
Independent versioned diagnostic targets preserve earlier experiments. They
are not a general-purpose guest CPU/device platform or an untrusted sandbox.

## Reproduce host verification

Run from repository root:

```
sh krnl/tests/linux_host_contracts.sh
```

Requires Python 3 and GCC-compatible x86-64 compiler with ASan/UBSan support.
Every check must succeed; the entry point does not silently skip failed tests.
The native harness checks CPL3 refusal, pure fixups and userspace FX-state
exchange only. No VMX entry, Linux guest, QEMU or firmware is launched.
Temporary test binaries are removed. Existing evidence is not deleted.

## Opt-in runtime build boundaries

`efi/linux-first-exit-recovery.mk` holds explicit diagnostic targets only.
Normal `make all` is unaffected. Reproducing those targets additionally needs
operator-provided fixed bzImage/initramfs and the matching historical EFI loader.
`LR_INPUTS` and `LR_BOOT_EFI` select them; expected hashes are checked by make.
These binaries are intentionally not committed. The recovery target is therefore
not a clean self-contained firmware build and host tests do not establish that.

The v12 high-PF runtime previously stopped before injection because the observed
PMD data flags included NX. The new independent NX profile admits only the
recorded flags and is host-tested, not runtime-verified. Previous v12 behavior
and evidence are not relabeled as success.

The independent UART target connects a staged polled COM1 TX handler with atomic
VMWRITE failure behavior. It accepts only byte DX IN/OUT; there is no RX, IRQ,
interactive terminal or demonstrated Linux shell. The newer NX/UART changes
have no Linux runtime test, per the instruction to finish features first.

No full Linux boot, M2 completion, firmware/storage/display coverage or general
hardware acceptance is claimed by this checkpoint.

## Additional host-only device foundations

`linux_msr_bank` adds staged guest FS VMCS handling and guest-owned
KERNEL_GS_BASE/syscall MSR storage. The isolated module is not wired to the
runner and does not pass guest MSR accesses through to host hardware.

`linux_uart` adds a separate 16-byte RX/TX model, IRQ/status side effects,
OUT2, loopback/modem state and four-character timeout. `linux_uart_model_io`
commits device/register effects only after the guest RIP VMWRITE succeeds.
The original TX-only runtime is unchanged. Input sourcing, VM-exit routing,
IRQ injection and Linux interaction are not wired or verified by these units.

The UART runtime bridge distinguishes rejection, committed I/O and committed
I/O with IRQ delivery pending. Callers must retry only IRQ synchronization,
never repeat an already-committed byte operation. Initialization requires a
fresh/quiescent context; the busy flag is not protection against arbitrary
trusted kernel callers. The bridge remains separate from live VM-exit/input
routing and does not establish working Linux interaction.
