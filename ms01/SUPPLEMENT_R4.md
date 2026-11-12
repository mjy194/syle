# MS-01 recovered-baseline host supplement (R4)

## Current environment and source identity

The latest original N48B/R3 working directories were absent in this execution
environment. The older `syle` directory was left unchanged. R3 delivery version 2
was recovered through the normal Library workflow and checked against archive
SHA-256 `dc2c7f0914ff980bf8ec4a8c54420d9d22411042c8dc1a460be26b15a8a87bcc`.
All 687 delivered source hashes matched its integration record.

This is a recovered baseline plus an isolated R4 working copy. It is not proof
that the missing original directory is current or that R4 was merged into it.
No production source is changed by this supplement. Current GCC/gcov is Debian
14.2.0-19. QEMU/GDB are not available through the unconfigured default command
paths used in the environment fingerprint; no guest-tool execution is attempted.
The existing EFI headers used by the successful host tests are under
`/workspace/shared/qemu-deps/root/usr`. This is configuration, not an installation.

## Fresh, independent host evidence

Only existing boot, ABI, console and production C UART host tests are expanded.
The production cores are compiled directly, with mocked firmware/UART boundaries;
there is no copied replacement implementation. Baseline/final comparisons use
the same flags and compiler, with retained raw gcov and sanitizer logs.

- Boot-related production sources: 233/233 instrumented lines before and after;
  taken branch edges improve from 291/340 to 334/340. Tests add missing callbacks,
  transactional config assertions, malformed map/allocation returns, excessive
  read counts, ELF header/order/overlap boundaries and exact rollback checks.
- Console: 63/80 to 80/80 lines; 72/92 to 92/92 branch outcomes. Exact GOP mock,
  snapshot, pixels, palette, cursor and scroll behavior is asserted.
- Boot ABI validator: 30/30 lines; 72/74 to 74/74 branch outcomes. Endpoint and
  readable-record range/containment checks are strengthened.
- UART/runtime C: 18/42 to 32/42 lines; 14/24 to 20/24 branch outcomes. Tests verify
  probe I/O ordering and exact CRLF/hex output. `kernel_runtime` remains unexecuted.
- Twelve selected behavioral mutations in test-local copies survive baseline
  assertions but fail the expanded assertions. These are artificial test checks,
  not twelve production defects or a general mutation score.

Coverage counts describe compiler-specific control-flow outcomes, not exhaustive
inputs, MC/DC, hardware behavior or milestone acceptance. One boot edge pair is
inserted by UBSan. Sanitizer-free exit-services separately reaches 48/48 outcomes.
Remaining boot edges are documented: redundant validation, the sanitizer divide
check, and two pointer-wrap guards deliberately not exercised with fabricated
pointers. Production invariants are not weakened to increase a percentage.

Exact commands, flags, hashes, baseline/final data and limitations are retained in
`evidence/r4-boot-host/` and the final console/ABI evidence directory identified in
`evidence/r4-final-summary.json`. The two standalone runners are:

- `python3 boot/tests/host_coverage.py --out <new-output> --kernel-elf <fixture>`
- `python3 efi/tests/ms01_console_abi_host.py --baseline <R3-source> --output <new-output>`

The optional ELF fixture is the SHA-verified delivered R3 `krnl.elf`, not a kernel
built in this environment. The default suite fixture hash is
`327519da1c5899dfd956ff05248f98022e2f9208c73cf415b3e40cb321c1612d`, 117640 bytes.
Only its format/load model is inspected; its machine code is not executed.

The fresh normal-script regression selects exactly these eight existing suites:
`provenance,boot-policy,boot-elf,boot-exit,boot-read,boot-abi,console,uart`.
Any `passed-selected-suites` result must not be presented as a full current QEMU
regression. The recovered R3 25-PASS/2-held and original 6/6 reports remain
historical evidence, explicitly separate from current execution.

## Held work is unchanged

- The paging/illegal-address runtime supplement was previously rejected by the
  execution platform; the exact denied operation was not identified. It is not
  retried, recreated, reworded or moved to another route here.
- The extra NMI emergency-UART budget runtime test was previously denied creating
  a QMP Unix socket (EPERM), before guest execution. No socket transport or path
  workaround is attempted. Ordinary production C UART mocks are independent of
  that emergency assembly/runtime test.
- Both held scripts remain absent. No PMM/paging, CR4/PAT, illegal-address guest,
  NMI-budget, QEMU, firmware or privileged kernel execution occurs in R4.
- No bare-metal, AP, timer, full first-32 hardware-exception, guest or JIT gate is
  added. Current board confirmation/candidate state is not changed.

## Aggregate configuration correction

The first eight-suite aggregate recorded 6 passes and two compiler failures:
console/UART could not locate `efi.h` with the unconfigured default prefix. Those
logs are retained in `evidence/r4-final-host`. The final host invocation uses the
already-present, verified EFI prefix used by the successful component runner,
and `UBSAN_OPTIONS=halt_on_error=1`. No software installation or permission change
is involved. The aggregate scope field now reflects selected layers instead of
unconditionally saying host plus QEMU. The final summary names the exact final
report; no failed invocation is silently relabeled or replaced.
