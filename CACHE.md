
## N28: shared UC lease across bounded dynamic contexts

The single-CPU registered UC contract is unchanged. A dynamic intrusive token
ledger now pins the lease once per initialized composed context, independently
of VMCS reference pins. Tokens bind item identity and the lease generation;
there is no fixed four-context token array. Each tree's typed admission checks
its own token, not merely a nonzero global count. RESET preserves tokens.
Successful tree destruction/INVEPT precedes token release and metadata free.
Candidate initialization before eviction may temporarily exceed the resident
budget; failed invalidation retains candidates, trees and their tokens.
Acquisition or typed-hook installation failure stops rather than returning a
fictitious successful lifecycle callback.

The trusted fixture uses three roots A/B/C with a resident budget of two,
sequence A/B/A/C/B/B/A, type-1 invalidation of B and type-2 invalidation of all
resident roots. All share two UC/IPAT data aliases to the same leased page;
WB page-table/code/stack leaves remain distinct. L1 executes real VMWRITEs and
INVEPTs, while L2 writes/reads 0x77..0x7d through alternate GVA aliases.
The fixture reuses private bootstrap metadata in unused high entries of root C;
only its low-GPA root entry is accessed. This is not arbitrary guest-table input.

Final VMCS02 CLEAR releases hardware context pins; each tree must successfully
flush/destroy before its token drops. VMCS01 is cleared, EPT01 detached/flushed,
and all lease refs must be zero before the established UC-to-WB host protocol.
Any failed destruction or transition retains the backing and stops. This remains
single-CPU UC/IPAT1, not concurrent registry mutation or other non-WB types.
Run `efi/tests/nested28.sh`; hardware validation is a separate isolated -smp 1 run.

## N29: whole-PAT admission, actual UC/IPAT0

The optional typed sparse mode now explicitly permits IPAT0 only when a trusted
backing callback proves compatibility. Old typed setters retain IPAT1-only
behavior. `nested_memory_backing_compatible` validates all eight PAT bytes and
requires every effective combination to match the registered physical type;
CD or NW is rejected. In particular EPT UC with PAT WC yields WC, not UC.
No single observed GVA is used to deduce a permanent GPA effective type.

This fixture retains UC type and IPAT0 in EPT02, with physical guest PAT loaded
and saved. PAT slots 0/1/2 are WB/UC-/WT; three GVA aliases select them and all
access the same registered UC backing. Other code/stack/paging EPT12 leaves
remain WB/IPAT1, while EPT01's UC view remains IPAT1. Two real L2 calls validate
0x77 and 0x99 without intervening INVEPT, preserving the cached data leaf.
L1 then changes only PAT slot7 to WC and attempts real VMRESUME. The adapter
requires an expected unsafe-PAT refusal before any physical02 entry; this is a
profile rejection, not fabricated VMfail7 or reason49. Final cleanup still
requires CLEAR/destroy/flush and restoration of the leased page to WB.

Every virtual entry validates the requested PAT/CR0; every physical02 entry,
including internal EPT fill resumes, rechecks its actual VMCS PAT/CR0 and
physical controls. Physical01 and physical02 CR0 masks guard CD/NW, with zero
read-shadow for those bits, so native MOV CR0 cannot silently change cache
policy. MSR bitmap control remains clear, so all RDMSR/WRMSR exit; unknown MSR
writes and CR-access exits fail closed. All-eight-slot proof is deliberately
conservative: a legal unused WC slot is still unsafe for this admitted profile.
No new physical memory type or multiprocessor transition is introduced.

## N30: existing-PAT WT registered backing

The registered service now selects a destination type and exact existing host
PAT slot, reusing the same alias audit and transition implementation. Only UC
and WT are exposed by this service. All PAT bytes must be legal and slot0 WB;
WT requires an exact byte4 slot. No PAT/MTRR MSR is rewritten. Absent WT or
unsupported platform topology yields BLOCKED before any transition. The prior
single-CPU, no-AP, private low page and sole 4K host alias contract still applies.
The existing audit additionally retains the platform's PAT3 UC requirement.

`vmx_memory_region_wb` is checked before remapping: its enabled-MTRR/fixed and
variable-range proof establishes the target's MTRR WB under this restricted
platform, not a general arbitrary-type MTRR resolver. WB MTRR plus the selected
WT PAT slot yields host WT. PTE indices encode bits3/4/7 (not huge-page bit12);
all 0x98 cache-index bits are cleared before installation. CACHE_PAGE_WT is a
separate state, with only WB↔WT and WB↔UC transitions allowed.

L1 EPT01 and composed EPT02 both use WT/IPAT1 for both target aliases, and the
host alias is WT. Page-table/code/stack backing remains WB. An opt-in sparse
WT setter preserves old UC/WB-only admission contracts. The single-context
fixture performs real nested writes/reads 0x77 and 0x99 across explicit INVEPT,
then clears VMCS references, destroys/flushed trees, detaches/flushed EPT01,
restores host WB and rechecks unchanged PAT and WB MTRR proof before release.
Logs expose host PAT/selected slot/cache bits and actual composed WT leaf fills.
CD/NW are masked on physical01/02; unknown CR/MSR modifications fail closed.
This is not WC/WP support, a PAT reprogramming protocol, or a multi-CPU cache
transition. Run `efi/tests/nested30.sh`; isolated -smp1 KVM is separate evidence.
