# Syle PM1 SCI control candidate (after run32)

## Base and isolation
Use a NEW directory and the exact actual run32 source from /home/javion/syle-ovmf-bootmenu-20261113.TK523q/source. Before copying overlay, verify BASE-SHA256SUMS in that source. Copy overlay krnl/ and efi/ only, then verify SHA256SUMS. Do not modify default checkout or prior frozen trees. This package contains delta, tests and review, not the full source/firmware/runner.

## Scope
Only quiet opt-in SYLE_FPM_SCI_CONTROL: B004 16-bit PM1 control read and SCI_EN latch write0/1. No PM1 event enables or SCI source/delivery, no SMI, sleep or DMA. All other control values, wrong widths and unimplemented ports refuse. Transaction sample effects persist, register/GPR publish only after RIP commit; failure fatal. All structure consumers (quiet observer, fpp, pmtimer) MUST be rebuilt with same macro. Old FC objects are removed from quiet link. Observer source and 6144/2MiB/30s reference limits unchanged.

## Fresh build / tests
Run `sh host/test.sh` (strict ASan/UBSan default and optional profile). Compare four host files to overlay/native copies byte-for-byte. Build with existing pinned OVMF hashes and EFI dependencies:
make -j4 -C SOURCE/efi -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk -f firmware-ovmf-clock.mk -f firmware-ovmf-apic.mk -f firmware-ovmf-cache-kvm.mk -f firmware-ovmf-irq.mk -f firmware-ovmf-quiet.mk EFI_PREFIX=EXISTING_DEP_PREFIX OVMF_CODE=PINNED_CODE OVMF_VARS=PINNED_VARS firmware-ovmf-quiet
Cloud static ELF sha a65ac5df50494243f3e487436042b7ce5729bccfa23ca292a0a3e3a233269256, 4977632 bytes, load span7630848. Different compiler may differ; record new hash. nm -u must be empty. Compile SOURCE/tools/quiet_loader_check.c with SOURCE/boot/kernel_elf.c and run on actual ELF; preserve original 5MiB file/8MiB span limits. Cloud original loader inspect/load and zero-fill/guards PASS, see loader-check.log. Verify final ELF only new SCI fpp/pmtimer objects and macro-consistent observer.

## Execution
Wait for any noDMA run to finish and verify no residual QEMU. RECONCILE LEDGER from actual previous runs immediately before run: confirmed run32 adds1 to33=34, but noDMA may increment later. Do NOT hardcode34 if another positive completed.
Use the existing reviewed run32 strict runner and same outer identity/hashes/QMP graph/KVM capability gates, CPU count1, no network or passthrough, tiny/noHv controls then at most ONE new positive. Retain 2MiB encoded trace, 30s admitted reference,6144 exits, existing VMX slice and outer180s timeout. No security/settings/host IRQ changes. Guest WBINVD budget at most1, may transiently impact host cache performance. Never restart an uncertain call; inspect process/fixture before deciding.

## Acceptance/evidence
Expected B004 actual66ED reads0, subsequent word write1 may set only virtual SCI_EN. Record actual instruction/opcode/state, next stop and all counters. No UI arrival assumed. Preserve raw/QMP/source/ELF/identity/media/host checks and cleanup evidence. Compact buffer is NOT exported: raw decoded count equality is not independent encoded native roundtrip. Overflow is incomplete and fails stop, no retry/expansion. Cleanup process/socket and archive even failure. Report ledger maintenance count and no residual VM.
