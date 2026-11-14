# Isolated guest NMI observations, independently verified

Two fresh single-vCPU QEMU guest fixtures executed, one READY and one RUNNING.
This is not a host/physical-machine NMI experiment. The frozen candidate used
kernel SHA256 ac33eaf3070868273679fcb765e66e602f51842a76635723bd9120e741cb245f.
Raw archive SHA256:
43797623556b7f555cf9888502c0f66c8c565bc4c3475c8c104d5aeace3d6d66.
The archived candidate bundle independently matches frozen SHA256
f8dc3daf0dbbb06df6985e928cbbe4085e18059b578607ff90f2fe61d6a7898d.

The durable independent decoded report and per-region SHA256 values are in
`evidence/nmi-independent-verification.json`. Raw binaries/logs remain in the
retained evidence archive identified above; this report does not replace them.

## Checked directly, not inferred from runner summary

For each mode, the QMP transcript contains exactly one guest inject-nmi request.
The isolated command uses -S, one vCPU and no guest NIC. The admitted manager
ELF/state and paused register snapshots agree with the expected wait site.

- READY: state 1, handle 1, saved CS 0x2b, RIP 0x8000000054.
- RUNNING: state 2, handle 1, saved CS 0x2b, RIP 0x8000000084.
- Each serial log reports exactly vector 2 and PANIC.COMPLETE. Post-event
  register snapshots are CPL0/HLT=1. Owner-exit counter remains zero.
- All six before/after regions compare byte-for-byte: full controller, actual
  LGM session, owner-exit counter, PMM stats, free-extent count and extent array.
- Independent decoding confirms prepared/enabled/host.active retained, 32,837
  RAM/EPT/control physical pages nonzero, aligned and pairwise distinct, and
  PMM allocated-page count 33,099 unchanged in each fixture.
- Both QMP runs acknowledge quit and the runner records their processes reaped.

Thus these two executions support the narrow behavior change: a CPL3 NMI
reaches fatal-stop without entering owner stop/release, preserving the observed
controller/page ownership state. Separate early admission failures/preflight
attempts are not counted as successful NMI tests.

## Cached TR/table observations and limits

Both modes show TR selector 0x18, cached base 0x43c6c00, limit 0x67 and busy-TSS
attributes 0x8b before and after. This matches THIS ELF's tss symbol (size0x68).
GDTR 0x43d6c70/0x37 and IDTR 0x43d6cb0/0xfff also match its private symbols.
These are useful actual virtual-CPU observations for these events.

They are NOT immediately-post-VM-exit host-cache snapshots: before injection
CS/SS are user selectors, and afterward NMI/IST entry has changed them (SS0 is
observed). Do not use them to claim post-exit CS/SS restoration, arbitrary future
TR state, nested NMI/IST robustness, console NMI safety, physical-host behavior,
multicore exclusion, mapping leases or DMA safety.

The newer exception-context gate/static-map candidate is a different artifact
and was not activated in these runs. Native bounded-map entry remains an
unconditional fail-stop. Its IDTR/descriptor checks and helper-zero result do
not become safe admission merely because the vector2 fix passed these tests.
