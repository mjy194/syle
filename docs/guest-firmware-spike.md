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
