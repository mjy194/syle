# Host-only PM timer / PIIX4 contract draft

No observer target links this prototype. The suspected absent-PIIX4→FFFFFFFF PMBA→port0006 chain still requires native logs and fetched instruction bytes. Port0006 is never implemented by this draft.

References: ACPI 6.5 section 4.8.3.3 (https://uefi.org/specs/ACPI/6.5/04_ACPI_Hardware_Specification.html), pinned edk2 2025.02 OvmfPkg/Library/AcpiTimerLib/BaseAcpiTimerLib.c, OvmfPlatforms.h and IndustryStandard/I440FxPiix4.h. The standard counter has a fixed 3,579,545 Hz rate, 24/32-bit width selected consistently with FADT, and dword read access. Highest implemented bit toggling latches timer status, even when SCI delivery is disabled.

The proposed restricted platform selects 24 bits, upper eight read bits zero, no SCI source. Future FADT publication must therefore leave TMR_VAL_EXT clear and identify the actual enabled address; no FADT has been published by this prototype. An internal timer-status latch records bit23 transitions but PM1 status/enable, SCI, SMI, sleep and other PM registers remain unimplemented/rejected. Enabling any unsupported event cannot be silently accepted.

Selected PIIX4 discovery mapping would be BDF00:01.3, PMBA at40h and PMREGMISC at80h. This draft models only exact dword PMBA and byte enable accesses. Reset is PMBA=1 (I/O indicator, no base) with decode disabled. Only baseB000 can be selected, followed by enable bit0; relocation while enabled and unknown bits/registers reject. Enabled timer port is B008, not0006. This is not a full PIIX4 device or a license to advertise arbitrary headers/capabilities. Device ID/header and any other register support require a separate consistent PCI integration review.

Time is derived from the same admitted Hv10MHz reference and epoch as RTC/APIC. PM ticks use quotient/remainder rational conversion, not the RTC's 1193182Hz units or the APIC's selected100MHz bus. All three devices can share an origin but have different rates and phases. RTC calendar still starts at declared synthetic2000-01-01; the PM counter has no date semantics. Counter advances with elapsed reference time while this running/S0-only guest exists, including host scheduling pauses, without claiming hard-real-time SCI delivery.

The host prototype checks epoch, reversal and max-gap; conversion cannot overflow because its ratio is below1. Counter wraps naturally at24 bits; internal full ticks detect even multiple bit23 toggles. Invalid accesses leave output/configuration unchanged; clock faults set a persistent failure. Native instruction/PCI transactions must stage configuration and preserve terminal ownership separately before publication. The current prototype has no GPR/RIP adapter and is not a native candidate.

Tests verify reset/base-before-enable, one-second rate, wrap and status latch, rejection of port6/wrong width/write/unknown config, epoch/reversal, and arithmetic at UINT64_MAX reference. ASan/UBSan and freestanding compilation pass.

## PCI and high-counter follow-up

The existing `fpi` fixture treats every BDF other than00:00.0 as absent. Connecting PM configuration must intercept exactly enabled BDF00:01.3 cycles BEFORE that fallback; unknown offsets at the newly present function must reject, not return all-ones as if the entire function were absent. CF8 address latch, CFC byte-lane/width rules and disabled-config cycles retain their existing semantics. A future header must describe an intentionally restricted PM function and must not claim bus mastering, SMBus, SCI, SMI or sleep capability. Other functions at device1 remain a separate unsupported platform dependency; this draft does not invent function0 enumeration behavior.

QEMU's PIIX4 model confirms PMBA's read-only I/O indicator initialized to1, address masking toFFC0 and decode controlled by config80 bit0 (https://qemu.googlesource.com/qemu/+/refs/heads/staging/hw/acpi/piix4.c). This is supporting compatibility evidence, not permission to copy its broader feature claims. The pinned EDK2 constructor reads80 first, programs40 when disabled, then enables80. This sequence fits the restricted reset1→B001→enable contract.

High-word tests now compare quotient/remainder conversion against a host-only 128-bit multiplication oracle at 2^32,2^48,2^63 and UINT64_MAX. Session-relative origin near UINT64_MAX is also tested. No 128-bit arithmetic is used in the freestanding model. Full internal ticks never truncate to32 bits before applying the24-bit register mask; timer-status detection uses the full tick quotient, avoiding a lost toggle after many wraps.
