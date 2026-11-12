# N44 bounded physical profile (candidate, not hardware evidence)

Build: source `efi/scripts/cloud-env.sh`, then `make -C efi smp-nested-atomic-largepage`.
Focused structural/fresh production build: `bash efi/tests/nested44.sh --host`.
Explicit TCG capability check: `bash efi/tests/nested44.sh --runtime` (never KVM).
Core semantic/sanitizer gates are separate in `nested44_core.py`.

The N43 target, payload, coordinator, and frozen acceptance evidence remain separate.
N44 adds opt-in virtual EPT capability bit 16 only. Physical EPT02 leaves remain
4KiB, so no physical 2MiB EPT capability is required.

The A/B roots share PDE18[1], a WB/IPAT1 AD1 2MiB EPT12 leaf mapping L1 base
zero. PDE0 retains the original 4KiB source PT for code, stack, mailbox, VMX
regions, and guest paging. C retains 4KiB alternate backing via PT27 at PDE1.
GVA f000 maps GPA20f000: A/B resolve L1f000 and C resolves L11c000.
GVA30000 maps GPA21d000: all roots resolve dedicated ordinary data L11d000.
This page is outside source tables16..27, C data28, and CPU1 private pages33+.
Independent catalog spans authorize each selected 4KiB backing; no contiguous
2MiB host allocation, complete range backing, or structural writable alias is
assumed. Source pages remain guest-readonly through both low and huge aliases.

The inherited 13-checkpoint/two-reset protocol uses two fixed physical owners,
private budget-two caches, actual guest reads/writes, retained cohorts, and
matching local INVEPT/maintenance receipts. Both selected offsets are separately
checked in physical EPT02. Secondary leaves remain read-only through shared
source D, acquire write permission only at checkpoints7/12, and are retracked
read-only after reset. Source audits and expected CAS counts use PS-aware AD
masks. No dynamic structural updates, permissions changes, reclamation, DMA,
or other VMs are supported by this profile.

Build and preprocessing success do not prove physical execution. Boundary,
backing-hole, and permission-denial coverage belongs to software tests unless
separately demonstrated on hardware. TCG CPUID_VMX0 is a capability skip only.
