# Opt-in firmware PAUSE completion

For the subsequent PM timer measurement, five-second Shell countdown and
isolated budget expansion, see [firmware-pm-delay.md](firmware-pm-delay.md).
The budgets described below belong to the original PAUSE-only trial.

The CODE ROM candidate stopped on VM-exit reason 40 at RIP `0x70c236e`.
The firmware reset plan already enables primary PAUSE exiting (bit 30), but
the quiet observer had no handler. The direct-Linux handler has a different
CPU/control contract and is not reused.

`firmware_pause.c` reads the stopped VMCS and checks raw reason 40, length 2,
primary PAUSE/secondary-control enable bits, secondary controls exactly `0xc2`
(EPT, WBINVD exiting, unrestricted guest; no PLE), IA-32e entry, clean debug
state, and no pending event or interruptibility. The existing owned-memory
walker admits only the CPL0 flat long-mode firmware profile and fetches the
actual instruction bytes, which must start with exactly `F3 90`. No fixed
RIP whitelist or exit-qualification operand is used. Noncanonical/overflowing
next RIP, truncated instructions and unsupported prefixes are rejected.

Successful completion clears RF when set, writes RIP+2, then increments a
diagnostic completion counter. Other flags and all GPRs/memory remain unchanged.
A callback may mutate VMCS before reporting failure: either failed write latches
terminal failure, and reentrant commit also fails terminally. The observer
stops on every rejection/failure. Its existing VMCS/EPT identity, host recovery,
IRQ receipt, clock sampling, next-entry and teardown gates remain in place.
There is no host sleep, synthetic tick increment, change to PAUSE interception,
or extension of the existing execution/trace/time budgets.

Architectural basis: Intel SDM, PAUSE instruction entry (Vol. 2), PAUSE exiting
and VM-exit instruction length (Vol. 3), and RF instruction-completion rules.
See the [Intel SDM](https://cdrdv2-public.intel.com/868137/325462-089-sdm-vol-1-2abcd-3abcd-4.pdf).
The permitted delay is not a calibrated clock or scheduler service.

## Build and host verification

From the repository root:

```sh
sh krnl/tests/firmware_pause.sh
sh krnl/tests/firmware_code_rom.sh
sh krnl/tests/firmware_irq_receipt.sh
sh krnl/tests/firmware_preemption_resume.sh
make -C efi -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk \
  -f firmware-ovmf-clock.mk -f firmware-ovmf-apic.mk \
  -f firmware-ovmf-cache-kvm.mk -f firmware-ovmf-irq.mk \
  -f firmware-code-rom.mk -f firmware-pause.mk -f firmware-ovmf-quiet.mk \
  firmware-pause EFI_PREFIX=/path/to/gnu-efi/usr \
  OVMF_CODE=/path/to/OVMF_CODE_4M.fd OVMF_VARS=/path/to/OVMF_VARS_4M.fd
```

The new target produces `efi/build/krnl-firmware-pause.elf` and a distinct
`fpause-observer.o`. Neither the normal quiet observer nor CODE-only observer
enables the handler. Host tests cover both RF states, all VMREAD failure points,
control/mode/debug/event rejections, every alternate opcode byte, 4KiB/2MiB
translation, a cross-page truncated instruction, partial-write and reentry
failure, and byte-for-byte unchanged backing. ASan/UBSan and freestanding
compilation are required. Successful host tests/builds alone are not firmware
runtime acceptance.

The checkout's older platform configuration remains distinct from the actual
PM1/CODE-ROM runtime base. A native candidate must preserve the latter's PM1,
DE, compact trace, PCI and fw_cfg settings. The native candidate for this change
copies the verified 1,551-file CODE-ROM source manifest, adds only the PAUSE
module/test/makefile and three observer insertions, and rebuilds all objects.

## Native result, 2026-11-14

The isolated candidate is retained at
`efi/build/firmware-pause-20261114._jzfs3h2/`. Its `summary.json`, source/media
manifests, build logs, original-loader check, control results and raw serial/QMP
records distinguish this runtime base from the checkout build.

- Fresh tiny reset16/protected32/long64 control exited 33 with cleanup; no-Hv
  control exited 35 at clock-source admission. Neither attempted WBINVD.
- Positive QMP device graph and installed/loaded KVM module identity checks
  passed. One continuation ran for 136.183 host seconds and exited 35, without
  outer timeout or automatic retry.
- Both immutable CODE ROM stages committed and retained original byte `0x78`.
- 477 PAUSE completions at RIP `0x70c236e` each logged actual `F3 90`, length 2,
  primary `0xe599fff2`, secondary `0xc2`, result 0 and terminal 0. Flags were
  `0x202`, `0x206`, `0x212` or `0x216`, with RF clear in every observed case.
  Completion counters were consecutive 1..477. These runtime cases had RF
  clear; the RF-set completion path is covered by host tests.
- The run reached the unchanged **6,144-exit budget**. The final B008 dword
  PM-timer read was accepted, and RIP advanced from `0x70c231b` to `0x70c231c`.
  The observer's generic `failed reason=0x1e` line describes the last exit; it
  is not evidence of an unsupported I/O operation. No new unsupported exit was
  reached. Firmware boot/UI and delay completion are not established.
- Trace-reported failed/incomplete/dropped were zero; logical/decoded bytes
  agreed at 4,119,416, and encoded bytes were 1,645,181 within the unchanged
  2 MiB buffer. The raw encoded buffer was not exported for independent replay.
- Cleanup completed and the QMP socket disappeared. Exactly one architectural
  L1 WBINVD attempt was logged (recorded cumulative count 38→39, not a count
  of physical cache flushes).

Native kernel SHA256:
`d3e89b99b21ecf53f23e3f1c7a9989a92215fd3fa778675db34bba6d4b9d80c3`.
Raw serial SHA256:
`1747ce75c121572c19d3e423615aef5a4805d1c172ac79426af4ce2fa0f7ea22`.
The original loader accepted file size 5,131,128 and span 7,675,904; no loader
limits changed and the ELF had no undefined symbols.

Frozen `evidence.tar.gz` SHA256:
`905fc677046d7f2a5b8f35831fae21eeb11e1ba1ba0a888de0f8141befe1d1b1`.

Next, bound and measure the PM-timer delay loop's requested duration and progress
before changing observation policy. A larger exit cap alone can exhaust the
fixed trace buffer; skipping timer reads or inventing elapsed ticks would alter
guest semantics. Subsequent experiments need a separately named bounded profile
and their own evidence. This result removes PAUSE as an unsupported instruction;
it does not complete milestone M2-S3.
