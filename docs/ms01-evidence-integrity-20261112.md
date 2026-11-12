# MS-01: prevent stale acceptance after runner failures

Base tree: `49055d54ea89a4829e80848105de13ba4357cf90`, the previously verified
two-round supplement to dev commit `8d9679e`. Work is isolated from both prior
delivery trees and remains unpushed.

## Actual defect and minimum correction

A host-only reproduction seeded `report.json` and `scenario-results.json` with
a previous successful invocation, then mocked the first `subprocess.Popen` call
to raise `FileNotFoundError`. The original runner terminated without replacing
either file: both still advertised PASS. No real subprocess was started. This
was a stale-evidence defect, not a kernel fault or a failed guest test.

The runner now:

- Replaces previous acceptance with a fresh `runId` and `running` status before
  hashing source, collecting tool versions or launching a selected command.
- Writes each JSON snapshot through a temporary file and atomic replacement,
  preventing readers from observing a partially rewritten JSON document.
- Records a failed command launch as `launch_error`, including the exception
  type/message, and counts it as a failure. It makes no retry or fallback attempt.
  A completed earlier suite from this same invocation remains recorded.
- Adds the same `runId` and terminal status to the scenario result. The terminal
  report is published last, after the scenario result is successfully written.
  An unexpected setup or mapping exception leaves incomplete current snapshots,
  rather than a previous PASS or a premature new PASS.

`running` is deliberately an incomplete evidence status, not a liveness check.
After an abrupt interruption it can remain in the output directory. Consumers
must require a terminal report and matching scenario `runId` and `status`; they must not
combine files from different invocations or interpret a partial snapshot as
milestone acceptance. Atomic file replacement is not a multi-file transaction,
a concurrency lock, or a hermetic-build guarantee. As before, run only one
aggregate in a given output directory at a time.

The report's terminal status is the commit marker. If writing it fails after
the scenario file is ready, the scenario may say PASS while the report remains
`running`, even with matching IDs. That pair is incomplete. A scenario's status
or `linked_suites_passed` field alone never commits acceptance.

## Focused regression

Six additional cases extend the existing `suite-contract` suite:

1. A missing executable invalidates old PASS files before its sole launch attempt
   and produces a final failure with incomplete scenario evidence.
2. A simulated denied launch after a successful suite preserves only this
   invocation's completed result and records exactly one attempt per command.
3. A setup/provenance exception invalidates old acceptance without starting any
   command.
4. A final mapping failure cannot publish terminal PASS, even when its selected
   suite returned success.
5. Two successful invocations have distinct IDs, internally matched report pairs
   and no leftover temporary JSON files.
6. Failure of the final report replacement leaves the authoritative report
   incomplete even if the same-ID scenario file is already ready, with no
   leftover temporary JSON files.

All process launches and tool-version collection in these new cases are mocked.
The existing held-selection regression still requires zero subprocesses for
both held IDs. No denied operation is recreated or retried by these tests.

## Scope review and remaining limits

A separate read-only review of the first milestone's boot/ELF/ABI contracts found
no new high-value production defect: the current tests already exercise complete
ELF prevalidation, no-write rollback on later invalid segments, fixed-address
allocation failure, source/destination overlap, BSS and size boundaries, bounded
final memory-map/EBS handling, and actual EFI status conversion. Adding another
suite merely for a thin file-protocol adapter was not justified by that review.

This change affects evidence bookkeeping only. No loader/kernel production C or
assembly changes, new scenario requirements, or new suite IDs are introduced.
The paging-boundaries and optional NMI UART-budget holds remain exactly as before.
No guest or QMP operation is run for this follow-up; bare metal and later-stage
features remain outside first-stage gates. A selected host pass is not full
MS-01 acceptance.

Final validation uses the existing toolchain and registry:

```sh
source efi/scripts/cloud-env.sh
export UBSAN_OPTIONS=halt_on_error=1
python3 ms01/run.py --group host --output ms01/evidence/integrity-final-host-reviewed
```

The resulting `report.json`, matching `scenario-results.json`, source hashes and
suite logs are retained with the delivery. Check that exact invocation's status
and counts rather than relying on an earlier report.
