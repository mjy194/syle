# Dev follow-up: diagnostics and contract regression tests

Baseline: `def8df1091e8d2752242dd9681f9c2c1b4445005`.

## Changes

- Kernel `AllocatePages` failures retain the full EFI status and the requested
  address/size captured before firmware can overwrite the address argument.
  Existing error categories, address validation and rollback remain unchanged.
- Final memory-map/ExitBootServices failures report the last transaction stage,
  raw EFI status, normalized transaction status and whether firmware failed or
  the transaction rejected its output. Cleanup cannot overwrite that record.
  The map-to-exit window contains no new logging or allocations.
- The real page-permission walker has host-only CR3/CR4/PAT observation hooks.
  Combined seal/split tests exposed a conservative false rejection: a new
  user-enabled intermediate table still leads to supervisor-only child leaves.
  The walker now computes effective supervisor permission across the path.
  Present/RW at every level, existing cache/PAT checks, translation matching and
  CR3/CR4 checks remain required. This is not evidence of an exploitable
  permission bypass or a current low-address production trigger.
- EPT API comments state that post-reset callbacks run after lower tables are
  freed and generation advances. Failure poisons the registry, not rollback.
  Preconditions belong to begin/pre_reset; consumers needing a post-flush
  release receipt must use the guarded sparse API. Filled-tree tests cover
  callback failure and pre-reset rejection. Production reset behavior is unchanged.

## Host-only reproduction

Set `EFI_PREFIX` to an installed GNU-EFI prefix and `UBSAN_OPTIONS=halt_on_error=1`.
Pass a built kernel ELF to the first command as a read-only parser fixture:

```sh
bash efi/tests/firmware_diagnostics.sh /absolute/path/to/krnl.elf
sh krnl/tests/paging_rw_wb.sh
sh krnl/tests/paging_alias.sh
bash krnl/tests/nested_context_transactions.sh
```

The tests use production adapters/walker/reset code with firmware or hardware
observations mocked. The EFI fixture covers 17 final-map/EBS scenarios and
3 kernel-allocation/rollback scenarios. Synthetic page-table frames are not
executed or accessed as guest memory. These host checks do not establish VMX,
guest large-page permissions or bare-metal behavior.

## Follow-up runtime regression

An independent clean-source verification also passed the existing QEMU TCG/OVMF
smoke script, the default production boot through Ring 3 management exit to
kernel idle, and both cases in `efi/tests/ms01_efi_failure.py`. The latter checks
persistent stale EBS (three attempts with fresh maps), a nonretryable EBS error,
full raw status output, no other Boot Services/kernel transfer after failure,
and the actual CLI/HLT terminal state. This is deterministic stopped-guest
firmware fault injection, not physical or partially torn-down firmware testing.

QEMU reports VMX unavailable. The paging walker/seal/split and EPT reset details
remain host-composition evidence, not hardware EPT or large-page guest coverage.
Previously held paging-boundaries and NMI UART runtime supplements are not
included or resumed; no full MS-01 aggregate was run or new gate introduced.
