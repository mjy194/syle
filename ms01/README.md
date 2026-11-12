# MS-01 implementation and reproducible validation

The latest isolated follow-up starts from verified dev commit `8d9679e` and adds
aggregate registration for the existing EFI diagnostics/page-permission host
checks, frame-layout regression and non-executable-held-suite checks. Its fresh
15-suite build/host result and static noncanonical-branch diagnosis are recorded
in [the follow-up report](../docs/ms01-followup-20261112.md). No guest execution or
complete milestone acceptance is claimed; both runtime holds remain unchanged.

The subsequent [stateful PMM supplement](../docs/ms01-pmm-stateful-20261112.md)
adds public-API ownership/model checks and metadata-split failure recovery. With
that host-only suite registered, the selected build/host aggregate has 16 suites.

The [evidence-integrity fix](../docs/ms01-evidence-integrity-20261112.md) prevents
an unavailable test executable from leaving an older PASS in the output folder.
Each invocation invalidates old acceptance before work starts; report and scenario
files carry the same fresh `runId`. `running` means an incomplete snapshot, not
proof that a process is still alive. Only a terminal report and matching scenario
file describe a completed invocation. The selected build/host suite count remains
16; no new milestone requirement is introduced.

Current R4 work uses a verified recovery of the R3 delivery, because the latest
original working directories are absent in this environment. R4 adds host tests
only; it does not claim a fresh original-directory merge or QEMU run. See
[SUPPLEMENT_R4.md](SUPPLEMENT_R4.md) for current evidence and unchanged holds.

Development started from the latest locally available `syle-n48b-working-20261111`
copy. It has no upstream Git metadata; it is not claimed to be the latest remote
commit. Backups and historical evidence are preserved; the integration report
records the exact files applied to the original working copy.

The prior R3 independent additions and held work are documented in
[SUPPLEMENT_R3.md](SUPPLEMENT_R3.md); the earlier gaps remain documented in
[SUPPLEMENT_R2.md](SUPPLEMENT_R2.md). The paging supplement is explicitly held
and cannot be executed by the aggregate runner. An additional NMI UART-budget
runtime attempt is also held after a QMP socket permission error, with no command. An all-suite run reports
`partial-blocked`, rather than presenting the independent passing suites as a
complete milestone acceptance.

## Implemented gaps

- BSP 256-entry IDT and matching assembly stubs, independent normal/#DF/NMI IST,
  stack-free early reentry guard, NMI spare-stack pivot, terminal IF=0/HLT policy.
- Full UART panic report before best-effort framebuffer output, bounded UART
  failure disable, early invalid-ABI diagnostics and invalid-GOP normalization.
- Managed physical-page W^X sealing across all owned-root aliases before code
  publication, including large-leaf splitting/PAT preservation and BSP full-TLB
  invalidation. Bootstrap user code no longer retains a writable kernel alias.
- Bounded final memory-map/EBS transaction, no unsafe firmware callbacks after
  an EBS attempt, shared ABI validation, production file-read/allocation test seams.
- Automated tests linked to all 64 MS-01 board scenarios, without changing
  their candidate/selected-draft status or treating all possible variants as tested.

`scenario-test-map.json` uses verified v22 board IDs. The two MS-02 Ring3
candidates are excluded. Existing Ring3 runs are extra regression coverage.

## Run

Use a GCC/binutils, GNU-EFI, QEMU/OVMF and GDB installation. Set `EFI_PREFIX`,
`QEMU`, `GDB`, `OVMF_CODE`, `OVMF_VARS`, `QEMU_DATA`, and `VGA_ROM` if not installed
at their standard locations. Existing `efi/scripts/cloud-env.sh` accepts a custom
`DEPS_ROOT` for a locally extracted toolchain. No network/install is performed.

From the source root:

```
python3 ms01/build_coverage.py
python3 ms01/run.py --group all
```

The runner captures commands, stdout/stderr, exit codes and source hashes in
`ms01/evidence/aggregate`. It fails if a suite fails or source changes during the
run. Individual suite IDs can be selected with `--only` and a separate `--output`.
Use one aggregate at a time. Diagnostic/debugger suites snapshot ELF/ESP inputs;
other historical scripts share mutable build paths and must run sequentially.

## What the layers mean

- Host tests exercise production C logic with injected firmware/UART callbacks,
  guarded memory and sanitizers. The alias walker uses UBSan; other listed
  sanitizer suites use ASan+UBSan. LSan is disabled under ptrace.
- QEMU TCG executes CPU faults and memory permissions. GDB adverse-state/code
  injection is explicitly identified, not passed off as normal boot behavior.
- Static all-vector checks validate all 256 emitted entries and error-code frame
  shapes. Representative exceptions, NMI, #DF and fallback run on the CPU; not
  all reserved/special vectors are individually hardware-triggered.
- The NMI repeat guard test forces adverse entry state. It is not an exhaustive
  proof over silicon NMI timing. Fatal handlers never IRET, so an accepted NMI
  remains blocked until external reset.

## Scope limits

First milestone remains x86-64, BSP-only, boot/early kernel. AP/IPI, kernel
periodic timers, guest VM/Windows and display after GPU takeover are not added.
No native physical firmware/CPU validation is claimed. QEMU RAM tests cover
256 MiB, 5 GiB and 8 GiB; 1/32 TiB maps and the exclusive 128 TiB direct-map cap
are synthetic/design checks, not tested machine capacities.

W^X guarantees the current owned kernel tree and inherited bootstrap user tree.
The sealing API refuses after AP startup or under unsupported translation state.
It is one-way publication, not a runtime JIT writable/executable toggle, arbitrary
external roots, EPT, or multicore shootdown API. The firmware-owned mappings
before the owned-CR3 checkpoint remain outside the post-handoff W^X contract.

Full exception protection begins after `exceptions_init` installs the IDT. The
brief inherited firmware GDT/IDT transition and unreadable arbitrary boot pointers
remain handoff preconditions. Best-effort output cannot guarantee visible text
when both devices or the active instruction/stack mappings are physically broken.

See `memory-coverage.md`, `../boot/MS01_AUTOMATION.md` and per-scenario scope limits.
