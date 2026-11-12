# MS-01 independent coverage supplement (R3)

This continues the R2 work without executing or recreating the held paging /
illegal-address branch. No production source or confirmation state is changed.
It does not introduce new acceptance gates for candidate details.

## All BSP fallback vectors

`efi/tests/ms01_fallback_vectors.py` executes an actual CPL0 software `INT` for
every vector 32–255, before the later Ring3 gate-128 change. For each of the 224
vectors it checks the unique gate/stub target, present DPL0 interrupt gate,
selector/IST, raw CPU frame, vector/synthetic error, exact saved RIP, all 15 GPRs,
normalized C frame, stack bounds/alignment, guard transitions, and one complete
serial panic report. The common terminal path is reached; after CLI, IF=0 and
the next instruction is HLT. It stops before executing HLT and does not claim
sustained halt for each case.

The debugger explicitly rearms registers and panic guards between fatal cases.
This is test intervention, not a kernel recovery or IRET implementation. Input
IF is zero. No hardware IRQ/controller routing, AP behavior, or all first-32
exception semantics are inferred. SC-M1-049 now has all 224 fallback entries
executed, while SC-M1-045 still has representative semantic exception tests.

The runner uses an exclusive build/suite lock, immutable private ELF/ESP and
firmware inputs, a private debugger port, bounded execution and owned-process
cleanup. Atomic reports distinguish focused subsets, failures and unexecuted
cases instead of retaining an earlier full PASS.

## Diagnostic harness correction

The first aggregate exposed a debugger-control failure in the existing #DF case:
`stepi` stopped at the same CLI again instead of advancing to HLT. Removing the
consumed breakpoint allowed five focused #DF repetitions to pass, but a later
original-directory fallback run showed the same lack of single-step progress
even after its breakpoint was deleted. The cause cannot be attributed solely
to retained breakpoints, nor is it a certified QEMU or kernel defect.

Both runners now assert arrival at the exact CLI address and the CLI/HLT bytes,
then continue the CPU to a breakpoint at the following HLT address, without
assigning guest RIP. Exact endpoint RIP, IF=0 and HLT assertions remain. This
observes the actual CLI transition without depending on GDB single-step behavior.
Original failures, intermediate repetitions and final validation are distinct
retained records. Production exception code and fault inputs are unchanged.

## Runtime provenance

Each aggregate writes `runtime-fingerprint.json`: selected executable versions
and hashes, OVMF/VGA firmware hashes, and selected GNU-EFI build inputs. It never
dumps the process environment. Missing tools/inputs and failed version commands
are explicit. Five host-only unit tests exercise hash/missing-file handling,
missing tools, timeouts/nonzero exits, fixed build-tool selection and bounded
output/environment exclusion.
This is useful provenance, not a hermetic or byte-reproducible-build guarantee.

## Additional NMI UART test work is held

A new, isolated status-injection runner was prepared for seven UART budget cases
(full ready, busy/absent first byte, busy/absent after a prefix, ready at the last
poll, and native no-UART). Syntax/build/static arithmetic and snapshot hashes
passed. No runtime case passed.

QEMU failed creating its private QMP Unix socket with `Operation not permitted`
before the guest executed. The 122-byte socket pathname also exceeds the usual
Linux 108-byte sockaddr_un capacity, but that fact does not establish the cause
of EPERM. The test was not retried via a shorter path, another transport or
another worker. Its unfinished script/evidence remain separate and are not
merged. The aggregate records `nmi-uart-budget` as held with no command.

This is additional validation work associated with 054/056, not a newly confirmed
mandatory gate. Existing production C UART tests and static emergency-assembly
budget checks remain their own evidence. Source mapping retains the core suite
results and separately exposes this unexecuted supplemental work.

## Other boundaries remain

- The earlier paging supplement remains held, with no executable command and
  no script in the merged source. Its six scenario gaps and failed noncanonical
  fetch RIP assertion are unchanged; see SUPPLEMENT_R2.md.
- Emergency SECOND_FAULT output is specific to NMI encountering an existing
  fatal-exception guard, not all ordinary panic recursion.
- 041–043 high-address extremes remain synthetic; QEMU RAM evidence is only
  256 MiB, 5 GiB and 8 GiB. No bare-metal/AP/timer/guest/JIT gate is added.
- Scenario-to-suite links do not prove exhaustive acceptance or alter the
  current board's confirmed/candidate status.

The full runner explicitly reports the two held groups and returns a nonzero
partial-blocked status even when every executable group passes. Its per-scenario
results refer only to that invocation; subset post-apply regression must not
replace the earlier full coverage record.
