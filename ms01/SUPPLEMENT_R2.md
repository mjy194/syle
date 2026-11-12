# MS-01 targeted coverage supplement

This supplements the previously passing 20-command baseline. The 64 scenario
associations and their candidate/selected-draft status are unchanged. No bare
metal, AP, timer, guest/Windows, or JIT requirement has been added.

## Completed independent additions

- **SC-M1-BOOT-05**: `efi/tests/ms01_efi_failure.py` executes the actual EFI
  adapter/controller with deterministic failed ExitBootServices returns injected
  at the firmware entry in guest RAM. Persistent stale-key and nonretryable
  device-error paths verify exact output/status, retry/key forwarding, forbidden
  callbacks, no post-EBS/kernel transfer and actual CLI/HLT terminal behavior.
- **047 / 048 / 056**: `ms01_nmi_overlap.py` requests real QEMU NMI while an
  ordinary #UD is active: after frame save, during UART, after full UART output,
  and without UART. All 176 bytes of the first frame, the NMI hardware frame,
  scratch registers and disjoint ordinary/NMI/spare stacks are checked.
  The NMI-overlap emergency path now attempts a stackless bounded
  `PANIC.SECOND_FAULT` UART marker, then safely halts. No framebuffer/calls/pushes
  occur. The marker is best effort and specific to NMI overlap, not every
  possible recursive ordinary fault.
- **063**: `ms01_debugger_abrupt.py` SIGKILLs the connected debugger, so it cannot
  send a graceful detach. QMP independently samples state after true TCP close.
  In this QEMU launch, a stopped target remains stopped and a running target
  remains running. Reconnect is checked; the stopped PC is unchanged. This
  documents existing tested behavior rather than choosing a universal product
  policy on the user's behalf.
- **045 / 049**: the existing diagnostics runner adds actual divide error and
  breakpoint, plus software INT32/127/128/254 fallback representatives. INT255
  remains covered. This is still representative execution plus all-256 gate
  and static frame-shape inspection, not every vector hardware-triggered.

## Evidence precision

- EFI faults use guest-RAM return injection; no physical firmware failure is
  asserted. Page-table OOM is an allocator-return injection in the actual build
  path, not a giant real-memory-exhaustion workload.
- A real NMI interrupts ordinary faults at selected checkpoints. The earlier
  forced guard state remains explicitly synthetic. No unrestricted natural
  nested-NMI or exhaustive instruction-window proof is claimed.
- The first ordinary frame survives early overlap, but no first diagnostic has
  yet been serialized at that checkpoint. Mid/late checkpoints preserve their
  emitted prefix/full report. With no UART, no marker is required to appear.
- 041–043 exact 4 GiB boundaries, sparse 1/32 TiB maps and the exclusive 128 TiB
  cap remain host synthetic checks. QEMU RAM runs are only 256 MiB, 5 GiB and
  8 GiB; these different evidence layers are never interchangeable.
- Source `scenario-test-map.json` contains associations, not a hard-coded
  pending or passing execution status. Each run writes `scenario-results.json`
  next to its report, explicitly marking missing suites and source changes.

## Partially recorded, blocked paging supplement

The unfinished `ms01_paging_boundaries.py` is retained only in the separate
isolated/frozen evidence tree. It is not copied into the original working copy.
Its aggregate suite is a non-executable `held` record, not a validated runner. The execution platform stopped its worker for possible
cybersecurity risk without identifying the exact rejected operation. This branch
was not retried, rephrased or run by another worker.

Existing `efi/build/ms01-paging-boundaries/report.json` contains 16 prior PASS
records: injected table-allocation failure, unsupported SMAP/PAT, framebuffer
PTE/endpoints, null/hole read-write-fetch, noncanonical read/write and precise
text/rodata write plus stack/data fetch frames. These are preserved historical
component evidence, not a pass of the final script. The final script changed
before termination and has not completed a verified run.

The separate noncanonical-fetch log has #GP/error=0 followed by a failed RIP
assertion: actual saved RIP 0x0000800000000000 versus expected branch RIP
0x040004d1. CR2, remaining frame checks and final halt were not verified for that
case. Its origin is unresolved; it is not certified as a QEMU bug or a passing
known limitation. The final script's provisional KNOWN_TCG_LIMITATION handling
is unverified and must not be treated as acceptance evidence.

SC-M1-022/026/032/033/034/038 remain incomplete for this supplement's final
aggregate even though earlier baseline and component results exist. No result
is silently dropped or promoted to PASS.

The final independent regression command is:

`python3 ms01/run.py --group all --output ms01/evidence/r2-final`

The aggregate records 23 executable groups plus one explicitly held group and
returns nonzero for the incomplete overall request. Held groups have no command
and cannot be activated with `--only`. New runners own separate debugger ports,
firmware-variable files and immutable ELF/ESP snapshots. Final aggregate remains
sequential and terminates only its own process group on timeout. Each new runner
clears its previous component report before starting to prevent stale PASS use.
