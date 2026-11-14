# Advanced EPT qualification: observed SVR read

The disabled-APIC probe reached a real LAPIC SVR read atFEE000F0 with exit
qualification581. Bit10 is not an EPT access permission: Intel SDM Vol3C,
“Exit Qualification for EPT Violations”, defines it as guest paging RW
metadata when bit7(GLA valid), bit8(final translated address) and advanced
EPT exit information support are present. IA32_VMX_EPT_VPID_CAP bit22 reports
that support (Intel SDM Vol3D, AppendixA.10).

The active reader now receives that capability from the captured host VMX
caps, never guesses it from a nonzero exit bit. Without support, only prior
181/182 are accepted. With support, bit10 must equal the owned page walk's
cumulative RW. Bit9 must match its cumulative user/supervisor classification;
this narrow profile only accepts supervisor pages, so user-page qualifications
still stop. NX remains unsupported and bit11/other unknown bits are rejected.
Bits7/8 remain mandatory and the EPT access/permission fields remain unchanged.

Guest CR0.WP0 can permit a supervisor write to a read-only page, but must not
turn that page's reported RW metadata into1. The reader checks page attributes
independently of whether the access itself is permitted. No new mapping or
host memory access is introduced.

Tests cover capability absent/present, matching RW, leaf and upper-level RO,
WP0 metadata mismatch, supervisor/user mismatch and unknown bits. This change
uses actual exit evidence plus the architectural contract, not a loose mask
that discards unexplained qualification fields.
