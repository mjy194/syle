# Syle EFI Stage 0 verification

Passed:
- EFI PE32+ generation, subsystem 10, .reloc and .rodata retained.
- GCC warnings-as-errors build for normal and smoke images.
- Real QEMU TCG + OVMF boot from FAT EFI/BOOT/BOOTX64.EFI.
- ExitBootServices followed by direct UART output and expected debug-exit.
- GDB at reset vector, relocated DWARF symbols, hardware breakpoint at efi_main.
- GDB hardware breakpoint at stage0_post_ebs after ExitBootServices success.

Not tested / not implemented:
- Hardware virtualization, VMXON, VMCS, EPT, nested VMX, Hyper-V guest.
- Multi-CPU, faults/interrupt handling, independent page tables and stack.
- Secure Boot or physical machine boot.

Initial test exposed missing .rodata in the ELF→PE section allowlist; fixed,
rebuilt both images and reran boot and GDB tests successfully. Serial/log and
exit-code assertions caught this rather than treating exit code alone as pass.

## Screen/panic extension

Implemented own GDT/TSS/IDT, static exception IST and separate double-fault IST;
previous "no GDT/IDT" limitation is superseded. Normal execution still uses
firmware-established paging and stack; no general memory allocator added.

Verified in QEMU TCG/OVMF:
- Normal screen console + UART output after ExitBootServices.
- Real UD2 -> #UD vector 6 / error 0 -> preserved register frame / red panic screen.
- Real write at ffff800000000000 -> #PF vector 14 / error 2 / matching CR2.
- UART removed: #UD panic screen remains visible (QMP screenshot manually inspected).
- GOP/VGA removed: serial-only smoke still passes.
- GDB entry/post-EBS breakpoints regression passes.
- Host ASan/UBSan console bounds, padding, draw/scroll/wrap/panic tests pass.

Not verified: all CPU vectors, double fault injection, corrupt normal stack,
NMI reentry, SMP, physical UART/GPU, Windows graphics-driver takeover. No claim
that GOP framebuffer survives Windows mode switching/remapping. Panic never
returns; recursive panic stops rather than attempting unsafe recovery.

## Syle directory migration (2026-11-07)

Moved the complete EFI module from `efi-hv-stage0/` to `syle/efi/`.
Rebuilt with `make -B all smoke faults` from the new path; internal stage0
symbols and log markers remain unchanged as phase identifiers.

Passed again in the new directory:
- Warnings-as-errors builds for normal, smoke, #UD and #PF images.
- QEMU/OVMF smoke boot and exit code 33.
- GDB relocated symbols and EFI entry/post-EBS breakpoints.
- ASan/UBSan console memory tests.
- Normal/#UD/#PF/no-UART framebuffer capture and exception assertions.
- No-GOP serial-only smoke boot.

Migration run logs: `build/rename-{build,smoke,debugger,console,display,no-gop}.log`.
This verifies directory relocation, not new VMX functionality. The original
adjacent `efi-hv-stage0.tar.gz` remains an unchanged historical archive.

## EFI → krnl handoff v1 (2026-11-07)

Implementation: `../krnl/main.c`, `../include/syle/boot.h`.
The final successful-EBS memory map and observed firmware metadata are passed
through a static, versioned boot-info structure. No EFI service table is passed.

Rebuilt normal/smoke/#UD/#PF with warnings as errors. Final regression results:

- `tests/smoke.sh`: PASS; QEMU TCG exit 33, ordered `SYLE.KRNL_ENTRY` / `BOOT_INFO.V1`
  markers, nonempty map, loader/kernel image-base agreement.
- `tests/debugger.py`: PASS; original EFI and post-EBS breakpoint symbols retained.
- `tests/display_faults.py`: PASS normal, #UD, #PF, #UD with UART absent.
- `tests/console_memory.sh`: PASS under ASan/UBSan.
- `NO_GOP=1 scripts/run.sh smoke`: exit 33; `SCREEN.UNAVAILABLE`, kernel entry and
  smoke-pass markers observed on UART.

Logs: `build/handoff-{build,smoke,debugger,display,memory,no-gop}.log`.
This validates an in-image kernel handoff on QEMU/OVMF only. It does not validate
ACPI table contents, independent kernel loading, reclaimed firmware memory,
VMX, Windows boot, physical hardware or post-Windows display takeover.

## 2026-11-08: first-stage boot chain

Supersedes the earlier “ordinary boot unchanged / diagnostic-only Ring 3” statement.
Default image now runs firmware-independent `boot` config/menu policy and loads a
standalone management ELF from the same ESP. The kernel validates the ELF and
switches to its own CR3/stack, enters CPL3, handles syscalls, and remains idle on
normal process exit. This is still one CPU without a scheduler.

Verified in QEMU TCG + OVMF (not VMX):
- Normal configured startup and missing-config defaults complete management exit.
- Diagnostic configuration and invalid-config fallback do not enter management.
- Missing management file fails before ExitBootServices; malformed ELF is rejected.
- No-GOP configuration completes the normal chain over UART.
- QMP-injected physical keyboard keys 1 and 2 override the opposite configured default.
- Legacy smoke, all five Ring 3 diagnostic variants, EFI/post-EBS GDB breakpoints,
  normal/#UD/#PF framebuffer capture, no-UART #UD, and console ASan/UBSan tests pass.
- Boot policy parser/menu and ELF loader host tests pass with ASan/UBSan;
  LeakSanitizer disabled because it is unsupported in this ptrace environment.

Artifacts: `build/chain-{e2e,smoke,ring3,debug,display,memory}.log` plus final
`build/chain-final-{build,e2e,ring3}.log` after last validation changes.
`tests/boot_chain.py` makes isolated ESP copies and never edits the default ESP.

The boot protocol is V2. No EFI service pointers cross it. `user_image` has its
own full allocation reservation range. EBS retains the no-allocation retry path.
Kernel is still embedded in the EFI image, not loaded as an independent kernel
binary. No general filesystem/allocator, runtime management UI, guest boot,
Windows, Secure Boot, physical GPU recovery or production security claim.

## 2026-11-08: independent kernel image (supersedes embedded-kernel limitation)

`BOOTX64.EFI` now contains only EFI adapter/boot policy/kernel ELF loader and
pre-boot diagnostics. `syle/krnl.elf` is separately linked ET_EXEC at 64 MiB.
No undefined symbols or relocations remain in the kernel; it links no EFI library.
All normal/smoke/fault/ring3 images load their own kernel ELF through the same path.
The protocol is V3 and records the distinct kernel reservation and entry.

The first proposed address 16 MiB was occupied by OVMF BootServicesData. Exact
AllocateAddress correctly refused it; the supported fixed base is now 64 MiB.
This is not relocation/KASLR. Any unavailable target fails rather than overwriting.
`kernel_entry` owns a 32 KiB startup stack. Paging remains firmware-derived until
normal Ring 3 startup replaces it. No memory reclamation is allowed.

Additional checks: missing/malformed kernel files fail before EBS, actual kernel
ELF loader host tests, GDB at independent kernel_entry and kernel_runtime in
addition to EFI/EBS, same default user process and earlier regressions.
Final logs use `build/separate-{e2e,smoke,ring3,debug,display,memory}.log`.

Independent-kernel verification result: 11 end-to-end cases, five Ring 3
isolation cases, smoke, GDB (including `GDB.OWN_STACK=1`), framebuffer faults and
console sanitizer all passed (`STANDALONE_STATUS=0`). All 9 generated kernel
ELFs pass the sanitizer-backed loader test. Clean source-only serial smoke build
and parallel `make -j8 all smoke faults ring3` also pass; missing build-directory
creation races were corrected with order-only prerequisites. After the final
no-reclaim diagnostic wording change, smoke and all 11 end-to-end cases reran
successfully (`RETEST_STATUS=0`).

## 2026-11-08 early memory ownership

Added conservative ConventionalMemory physical page allocator and early kernel
CR3 switching on every kernel path (before management or diagnostics). Kernel
ELF now has separate RX text, RO/NX rodata and RW/NX data/BSS/stack; the old Ring 3
RWX 4 GiB mapping was removed in favor of inherited supervisor kernel mappings.

New `tests/kernel_memory.py` checks actual QEMU #PF for supervisor text write
(error 3), NX stack execution (error 17), and null read (error 0). PMM host tests
cover invalid maps, reservations, unique allocation, exhaustion/free, and wrong
or duplicate frees under ASan/UBSan. The PMM smoke self-test exercises alloc/free
before installing its page tables. Test logs use `build/mm-*.log`.

Firmware memory remains reserved. Early construction still depends on firmware
identity access before the CR3 switch. No VMX/SMP/real-machine validation implied.
