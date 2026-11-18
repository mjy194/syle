# Syle M2 OVMF spike: fixed inputs and dependency findings

## Scope and evidence
This was a read-only audit of the current Syle checkout plus download/extraction of an official firmware package and fixed upstream sources. No Syle source, firmware configuration, host security setting, or guest was changed. No OVMF-under-Syle boot is claimed.

## Frozen firmware candidate
Source: https://deb.debian.org/debian/pool/main/e/edk2/ovmf_2025.02-8+deb13u1_all.deb
Package SHA256: 78e0d54df11fc77406cb7a0bc9a39e5bca6d1cbe06556b91d9a73491c52decdf

- OVMF_CODE_4M.fd: 3,653,632 bytes; SHA256 624e06de18b4fa535e90db7160d00d3d07d206422b89999bf1e27d920264e4e0
- OVMF_VARS_4M.fd: 540,672 bytes; SHA256 5d2ac383371b408398accee7ec27c8c09ea5b74a0de0ceea6513388b15be5d1e
- Combined size: 4 MiB. Contiguous top-of-32-bit-address-space layout gives CODE base 0xffc84000 and VARS base 0xffc00000. These are a candidate mapping, not a validated Syle mapping.
- Static binary reset-vector probe at 0xfffffff0 begins `mov eax,cr0; test al,1`, then branches according to PE. It is not a Linux long-mode entry.

Full extracted artifacts and firmware-inspection.json are in this directory. Source snapshot 8628578dacd38b327d4f9acf820c6914f4b3a48f is an independently pinned upstream reference, not claimed to be the exact source tree of Debian's patched 2025.02 firmware. Confirm release-specific behavior in the actual binary or matching source before relying on it.

## Hard first blockers in Syle
- linux_vcpu_prepare.c requires kernel_addr+0x200, fixed Linux boot registers, preconstructed long mode; secondary controls explicitly exclude unrestricted guest.
- linux_vcpu_cr.c and linux_vcpu_policy.c require PG/PE and PAE, fixed CR profile. Firmware mode transitions are not represented.
- linux_guest_memory.c maps only low 128 MiB private RAM; no high firmware ROM or flash variable range.
- CPUID guest profile hides APIC by default; direct Linux success does not establish OVMF compatibility.

Do not weaken these direct-Linux contracts in place. Add a separate firmware guest entry/profile and memory-map builder, with pure validation tests, then native probes.

## Platform dependencies from pinned upstream
OvmfPkgX64.fdf includes LocalApicTimerDxe, PCI host bridge/bus drivers, VirtioPci/Virtio10/VirtioBlk, flash services, QemuRamfbDxe. OvmfPkgX64.dsc uses CF8 PCI configuration, ACPI PM timer libraries, QemuFwCfg libraries and LocalApic library. Thus both PM timer delay support and LAPIC timer must be evaluated, not inferred from the existing PIT model. Driver inclusion alone is not evidence every device is required.

PlatformPei reads host bridge device ID and selects platform behavior. A fabricated ID without matching chipset semantics is unsafe. First freeze a narrow machine contract; do not claim generic Q35/i440fx support.

## ramfb finding
QemuRamfbDxe finds fw_cfg file `etc/ramfb`, checks its exact packed configuration size, allocates reserved guest RAM pages and writes a big-endian address/FourCC/flags/width/height/stride structure. The host must validate all addresses, size/stride multiplication, RAM ownership and supported pixel formats before consuming it.

Important: QEMU v10.0.0 hw/display/ramfb.c explicitly rejects missing fw_cfg DMA; docs/specs/fw_cfg.rst says data-port writes are ignored in modern QEMU and writes are supported via DMA. The EDK2 PIO fallback is not sufficient evidence of a compatible ramfb write path. Implement bounded fw_cfg DMA descriptors and guest memory copies if adopting ramfb. This is an emulated guest-memory protocol, not physical passthrough or VT-d.

ramfb provides GOP metadata, not host rendering. Syle still needs guest-to-host display presentation and Linux framebuffer handoff validation.

## Recommended probes and order
1. Separate reset-mode VMCS plan, high ROM mapping and controls. Test invalid capabilities, PG/PE/EFER consistency, ROM writes, holes and bounds.
2. Tiny reset->protected->long mode guest; verify native exit and state, preserving direct Linux regression.
3. Fixed OVMF CODE/VARS, private VARS copy per run; capture first unsupported I/O/MSR/CR operation with bounded execution. No disk or display yet; serialize debug output.
4. Freeze actual minimal platform contract: PCI CF8/CFC, host bridge fields, fw_cfg directory/DMA, PM timer, LAPIC, RTC/ACPI/flash behavior used by the chosen build.
5. Reach firmware shell/boot manager. Only then add virtio-blk PCI single queue and disk EFI loader/rootfs; test the block device first with direct Linux.
6. Add ramfb and host presentation, keeping serial console as independent diagnosis/input.

## Unverified
No actual firmware boot, virtio block operation, guest GOP/Linux display, firmware NVRAM persistence, or final M2 acceptance was performed here. The fixed candidate is an input for implementation, not evidence of compatibility.

## First implementation artifact
`firmware_layout.c/.h` is a pure candidate guest physical-map planner. Its three ranges are low RAM (RWX), flash VARS (R, writes trapped), and CODE (RX), ending at 4GiB. It rejects zero/unaligned/overflowing/overlapping layouts, reserves the low-to-flash hole, and proves the 16-byte reset vector belongs to CODE. `fwl_contains` performs subtraction-based range checks for future bounded accesses.

This does not map memory, allocate private host pages, emulate flash, build a reset VMCS, or launch firmware. Callers must separately establish unique owned backing, RAM holes/devices, firmware image validity, fresh private VARS contents, and safe teardown. The current contiguous low-RAM plan is an initial probe contract, not a complete PC E820/PCI layout. No existing direct Linux path calls the module.

Run `sh krnl/tests/firmware_layout.sh` for ASan/UBSan and freestanding compile checks.

## Native CODE diagnostic checkpoint, 2026-11-14

This later observation supersedes the initial audit's no-runtime status only for
the evidence below; it does not establish guest UEFI UI, disk boot or display.
After explicit user authorization lifted the recorded execution pause for one
diagnostic attempt, a fresh copy of actual PM1 source was checked against
`experiments/m2-snapshots/code-rom-diagnostic/BASE-SHA256SUMS`. Only the three
archived diagnostic source files were installed; the original source and
archived snapshots were not modified. All native objects were rebuilt.

Evidence directory on archbook:
`/home/javion/syle-code-diag-20261114.k2KxFj`.
It contains source/media manifests, build and original-loader checks, the
unchanged one-shot runner, control/positive serial and QMP logs, `experiment.json`
and machine-checked `summary.json`.

- Original loader inspect/load passed: file 5,104,376 bytes, span 7,671,808 bytes,
  zero-fill 3,271,676 bytes, guards intact; undefined-symbol output empty.
  Existing 5 MiB file and 8 MiB span limits were not changed.
- Fresh tiny control passed reset16/protected32/long64 and cleanup (exit 33).
  Fresh noHv control rejected clock-source admission (exit 35). Neither attempted
  architectural L1 WBINVD.
- One positive run passed the preserved KVM/module/QMP/device-graph checks and
  exited 35 after 52.137 host seconds, with 5,169 completed dispatches. This exit
  denotes the expected diagnostic stop, not firmware boot success.
- At RIP `0x70af26b`, reason `0x30` (EPT violation), qualification `0x5aa`, actual
  owned instruction bytes began `c6 00 50 8a 10 80 f9 50 75 3e c6 00 70 40 8a`.
  The first instruction is `mov byte ptr [rax], 0x50`.
- RAX, GLA, GPA and independently walked read/write GPA were all `0xffc84010`.
  The guest mapping admitted writes, including the diagnostic A/D requirements;
  the guest leaf was a 2 MiB PDE `0xffc000e3`. Vectoring was zero.
- CODE ownership and original-byte reads succeeded; original byte was `0x78`.
  All four EPT levels matched the expected owned chain, with low entry bytes
  `7/7/7/0x35` (CODE leaf WB/RX). This supports the ROM-probe interpretation,
  not a guest page-fault or permission to make CODE writable.
- The stop-only marker was observed exactly once; no store or instruction commit
  was performed. The following read and `0x70` probe were not executed. Logged
  RFLAGS were `0x10206`, so any future instruction-completion path must handle RF
  correctly rather than blindly advancing RIP.
- Trace counters reported failed/incomplete/dropped all zero; logical/decoded
  bytes agreed at 3,305,980 and encoded bytes were 1,321,580, below the unchanged
  2 MiB limit. The raw encoded buffer was not exported, so this is not an
  independent native encoded-buffer roundtrip verification.
- Exactly one architectural L1 WBINVD attempt returned successfully, taking the
  reconciled recorded count from 36 to 37 (not a physical flush count). No retry.
  Guest teardown completed; no residual QEMU/runner or QMP socket remained.

Kernel SHA256:
`7b2ecd0b5dd78b3c5a76165e6077bbd581a5791e80cbd06322c434db472e194a`.
Raw positive serial SHA256:
`074a8d3f206eeb95765ffdb0f794ce7d401cb15f02b59f86f36fb397391de343`.

The next implementation gate is a separate, bounded CODE ROM probe transaction:
decode and validate the actual store, prove translation and RX backing, preserve
ROM contents, and commit architectural instruction state with fail-stop handling
of partial VMCS writes. Do not reuse VARS admission for CODE, treat diagnostic
success as execution approval, or infer completion of the second probe or
ReadyToBoot callback from this first-write observation.

Frozen evidence archive: `evidence.tar.gz` in the run directory; SHA256
`11be1052a5801ff206ce4f820e7d91e1698a8ae4b38340fe52e9d13ffd229fb2`.

## Opt-in CODE ROM transaction

`krnl/firmware_code_rom.c` now implements an independent immutable CODE probe,
not a relaxation of VARS or MMIO admission. It binds the first non00/50/70 byte
in the first64KiB CODE block and accepts at most byte50 then byte70 at that
address. Native reads between writes are not receipt-verified, and the caller
must not claim that both probes ran merely because the model supports them.

Each transaction decodes actual owned instruction bytes, validates the decoded
linear address against the exit and guest WRITE translation, checks the exact
CODE RX EPT chain and unique backing, and verifies the original byte is unchanged.
It then clears RF when set and advances RIP to the decoded next instruction.
The probe stage is published only after both required VMCS writes succeed. Any
VMCS write failure permanently latches terminal failure, including a callback
that writes before returning failure. Rejected transactions must also stop the
caller. No backing, EPT, guest page-table or GPR writes are performed.

The quiet observer calls this path only with `SYLE_FIRMWARE_CODE_ROM`. Include
`efi/firmware-code-rom.mk` **before** `efi/firmware-ovmf-quiet.mk`, after the
existing firmware-probe/observe/clock/apic/cache-kvm/irq makefile chain, and build
`firmware-code-rom`. Its observer object and `build/krnl-code-rom.elf` are distinct
from the normal quiet artifacts, preventing opt-in object reuse on a subsequent
normal build. All existing CPU, clock, input, trace and execution limits remain.
The normal checkout's older platform profile is not the actual PM1 runtime base.

Host validation: `sh krnl/tests/firmware_code_rom.sh` exercises ASan/UBSan and
freestanding compilation, including actual C60050/RF behavior,50/70 ordering,
unchanged complete backing,4KiB/2MiB guest leaves, permission/qualification and
alias failures, truncated instructions, and partial-commit terminal behavior.
The existing `firmware_vars_rom.sh` also passed unchanged. Both checkout opt-in
and normal quiet targets linked, with `fcode_mmio` absent from the normal ELF.

A fresh native candidate was prepared from the verified diagnostic source in
`/home/javion/syle-code-rom-20261114.3fj23B`, preserving its PM1, DE, compact-trace,
PCI and fw_cfg profiles. Its matching host CODE tests passed. Original loader
inspect/load passed with file5,121,464, span7,675,904, zero-fill3,271,676 and
intact guards; undefined-symbol output was empty. Kernel SHA256:
`8478a8c2fdabe8867ec5b541a9cda3efa1433569b97aa4357649c278e6ea4cda`.

### Native completion observation, 2026-11-14

After separate explicit authorization for one completion trial, fresh tiny and
noHv controls passed with exits33/35 and no maintenance attempt. The positive
run passed the preserved identity/QMP gates, completed5,194 dispatches and exited
35 in52.156 host seconds. At `0x70af26b`, actual `c6 00 50` committed stage1;
at `0x70af275`, actual `c6 00 70` committed stage2. Both targeted `0xffc84010`,
retained original byte`0x78` and reported terminal0. Fresh entry gates after each
commit showed flags`0x206` (RF clear). The second commit's admission rechecked
the original backing after the first; untrapped reads are not independently
logged by this model. CODE EPT permissions were not changed.

The guest then performed accepted PM timer reads at B008. The next unsupported
exit was reason`0x28` (decimal40, PAUSE) at RIP`0x70c236e`, qualification0,
VM-instruction error0. No actual instruction bytes were captured for this new
PAUSE stop; do not confuse its hardware exit classification with a decoded-byte
verification. No callback completion, UEFI UI, disk boot or display acceptance
is claimed. The one positive authorization was consumed; no retry was performed.

Trace counters reported failed/incomplete/dropped all0, logical/decoded bytes
3,325,229 and encoded bytes1,329,021, within the unchanged2MiB limit. As before,
the raw encoded buffer was not exported. Guest teardown completed, and no
residual QEMU/runner or QMP socket remained. Exactly one architectural L1 WBINVD
attempt returned successfully; recorded cumulative count37->38, not a physical
flush count. Runtime assertions and provenance are in the candidate directory's
`summary.json` and `experiment.json`; raw serial SHA256:
`164cea4f15ea69df0c10ef1a67b229f23f7ead5a24441913141360120110b7b4`.

Frozen `evidence.tar.gz` SHA256:
`870251e066b0229dfbbe544eb3614c18cad2ec1ec276b06108bf830568d47a80`.

## PAUSE completion follow-up, 2026-11-14

The independent `firmware-pause` target now validates actual owned `F3 90`
bytes, PAUSE-exiting controls and the CPL0 firmware execution state before
retiring RF/RIP, with terminal handling of partial VMCS writes. The ordinary
quiet and CODE-only targets retain their previous behavior.

A fresh rebuild of the verified actual CODE-ROM runtime base passed host tests,
original-loader validation, tiny/noHv controls, and one bounded KVM run. All
477 observed PAUSE transactions committed; CODE ROM stages1/2 remained intact.
The guest reached the existing 6144-exit budget while polling PM timer B008.
Its final I/O was accepted and RIP advanced; this is a budget stop, not a new
unsupported I/O exit or firmware boot success. Cleanup completed. The single
architectural L1 WBINVD attempt takes the recorded count38→39.

Detailed scope, artifact hashes and next diagnostic boundary are in
[firmware-pause.md](firmware-pause.md). Native evidence is retained at
`efi/build/firmware-pause-20261114._jzfs3h2/summary.json` with the source snapshot
and raw logs; this preserves the external PM1/DE/compact-trace platform rather
than claiming the ordinary checkout has integrated every earlier overlay.

## PM timer countdown follow-up, 2026-11-14

The previous 6144-exit stop was only 68.910 ms into a one-second Metronome
request. Further caller/binary inspection and read-only runtime snapshots
identified the complete target: UEFI Shell's five-second startup countdown,
with saved remaining values 5, 4, 3, 2, 1. An isolated sampled-log profile with
131072 exits completed all five waits (5.000177 s total measured wait time).
It then stopped on unsupported HLT at `0x70d09e0`, after 86431 dispatches,
below the unchanged 30-second and 2 MiB trace limits. Trace integrity and
tiny/noHv/native admission checks passed; cleanup completed.

This establishes execution beyond the Shell startup delay, not interactive
Shell or firmware-to-Linux boot acceptance. The next firmware boundary is
HLT idle/wakeup handling. Five native attempts in this follow-up move the
recorded architectural WBINVD ledger39→44. See
[firmware-pm-delay.md](firmware-pm-delay.md) for measurements, scope and evidence.

## HLT idle/wakeup follow-up, 2026-11-14

The opt-in HLT handler now completes the actual owned `F4`, retires RF/shadow
state, advances RIP once and enters VMCS HLT activity. It retains a software
wait across uninjected preemption exits and releases it only after the existing
IRQ receipt bridge confirms delivery. No new interrupt or synthetic time is
introduced.

One native run of the verified PM-countdown base completed four HLTs, 53 idle
slices, three verified vector32 wakes and three committed guest EOIs, returning
to the idle loop each time. It stopped intentionally at the bounded probe
condition after 86510 dispatches, with intact trace and clean teardown. The
former HLT rejection is resolved for this profile; interactive Shell and
firmware-to-Linux acceptance remain outstanding. The architectural WBINVD
ledger moves44→45. See [firmware-halt.md](firmware-halt.md).


## Serial console follow-up, 2026-11-17

The opt-in PCI 16550 console now reaches an interactive UEFI Shell through
ordinary firmware discovery. Native host serial input executed `ver` and
`echo SYLE_CONSOLE_OK_20261117`, each returning to the Shell prompt. The final
probe stopped deliberately with complete trace and clean teardown, after
fresh tiny/no-Hv controls and updated host KVM admission. See
[firmware-console.md](firmware-console.md) for the acceptance boundary,
intermediate failures and reproducible evidence. Firmware-launched Linux,
storage, display and Ring3 lifecycle remain milestone2 work.


### 2026-11-18: virtual disk -> UEFI -> interactive Linux accepted

The nondefault `firmware-disk` profile adds a read-only64MiB legacy virtio-blk
at00:03.0. OVMF reads its GPT/FAT32 ESP, loads the disk EFI application and
Linux EFI stub, then Linux6.12 reaches its embedded initramfs and BusyBox
Shell. Native serial commands `uname -s`, `uname -r` and a unique echo all
completed and returned a fresh prompt. The final evidence analyzer reports
PASS in `efi/build/firmware-disk-accepted-20261118/analysis.json`:36 disk
completions,338 IRQ0 receipts,47 IRQ4 receipts, all59 input bytes read, no
trace loss or disk failure. Controls, source/media hashes, exact process/QMP
admission and the previous runtime limits are retained. Full details and
scope are in [firmware-disk.md](firmware-disk.md). This accepts a read-only
boot disk and initramfs root; writable/persistent disks and Linux virtio IRQ
operation remain future work.


### 2026-11-18: writable disk root and reboot persistence accepted

The separate persistent profile uses a64MiB file-backed PIO provider under
writable virtio-blk, with FLUSH and fixed guest IRQ5. Linux's own PCI/virtio
stack mounts ext2 `vda2` directly; the new EFI kernel has no embedded
initramfs. A Linux userspace helper writes and fsyncs a random file, remounts
root read-only and requests reboot. A completely new QEMU/Syle/Linux instance
then reads the same token from the same backing inode. Ordered native and
offline evidence both pass, including matching file SHA-256 and two clean
fsck checks, in `efi/build/persistent-accepted-20261118/analysis.json`.
The restart is managed VM recreation; in-process VMCS reset remains separate.
See [firmware-persistent-disk.md](firmware-persistent-disk.md) for ownership,
limits, counters and reproduction.


### 2026-11-18: Ring3-managed firmware lifecycle accepted

The UEFI + writable disk runtime now uses the existing Ring3 management ABI.
In one native QEMU/Syle run the manager boots disk-root Linux, stops it, checks
256 pure STATUS calls and a rejected stopped STEP, resumes the same guest
with unchanged CPU/RAM/device digest and retained shell PID/variable, then
destroys it. A new handle boots UEFI/Linux again and reads the persisted file.
Both destroys return PMM pages to the exact 262-page baseline and clear VMX
regions, EPT and disk RAM access. Console input/output traverses the Ring3 ABI;
no host serial command is injected. Native lifecycle, controls, offline fsck
and source/media verification pass in
`efi/build/managed-firmware-remount-20261118/analysis.json`. See
[firmware-managed.md](firmware-managed.md). Guest GOP/framebuffer and host
presentation remain milestone2 work.


### 2026-11-18: guest GOP, Linux framebuffer and host scanout accepted

The Ring3-managed UEFI/disk profile now advertises one bounded ramfb file and
implements fw_cfg DMA through owned guest RAM. Actual pinned OVMF installs GOP;
the disk EFI application renders an 800x600 pattern. Linux's EFI framebuffer
driver inherits it and a disk-root userspace helper writes a distinct pattern
through `/dev/fb0`. QMP screenshots verify exact colors on the host boot GOP
framebuffer in `efi/build/graphics-no-ps2-20261118/graphics-analysis.json` (PASS).
The same run retains pause/resume with PID/variables, persisted file verification
after recreate, exact PMM balance and display/VMX/EPT revocation on both destroys.
This closes the earlier milestone2 display gap for the admitted single-vCPU
profile. See [firmware-graphics.md](firmware-graphics.md).
