# Memory and W^X scenario automation

This maps assertion evidence, not a claim that every draft detail is proven.
All QEMU results mean QEMU/OVMF TCG, not native hardware. Final aggregate logs
supersede the component logs listed here. No published board state is changed.

The later `pmm-model` host suite adds independent bitmap/ownership checks for
SC-M1-024 through SC-M1-029 across eight deterministic 25,000-step sequences,
plus allocation-split metadata-capacity failure and recovery. See
[`ms01-pmm-stateful-20261112.md`](../docs/ms01-pmm-stateful-20261112.md).
These synthetic public-API checks do not replace any held guest supplement.

| Draft MS1 scenario | Automated assertions | Limits |
| --- | --- | --- |
| 024 | `krnl/tests/memory.sh`: only conventional WB pages, loader/kernel/user/map/FB reservations, runtime/protected/UC and firmware exclusions | Synthetic host map |
| 025 | Same: unique allocations, free/reallocate and final statistics; `high_memory.py`: real high-page write/read/free | BSP only |
| 026 | Same: zero and single-page pools, exhaustion, release/recovery | Page-table allocation failure panics, not a QEMU OOM injection |
| 027 | Same: unaligned, reserved, out-of-bound, duplicate and unallocated free rejection | Host ownership assertions |
| 028 | Same: descriptor alignment, zero/overflow counts, overlap, malformed stride/buffer and sparse extents | Trusted input pointer contract remains |
| 029 | Same: >1024 descriptor count rejected before dereference; free-extent capacity failure preserves stats and ownership, recovery after merge | No claim of arbitrary firmware map capacity |
| 030 | `kernel_memory.py` and `wx_alias.py`: own-CR3 marker followed by expected hardware faults; normal/ring3/high-memory regressions continue running | Run in final aggregate |
| 031 | Normal/ring3/high-memory runs: code executes, data and stacks operate; `paging_init` checks RX/RO/NX leaf flags | Not every virtual address individually checked |
| 032 | `kernel_memory.py` case 1: supervisor text write #PF error 3; `wx_alias.py`: both aliases #PF error 3, exact CR2 | Extended case 4 separately faults on rodata writes |
| 033 | `kernel_memory.py` case 2: stack instruction fetch #PF error 17; ring3 case 3 tests NX user stack | Extended case 5 separately faults on data-page instruction fetch |
| 034 | `kernel_memory.py` case 3: null read #PF error 0; existing fault path exercises absent direct-map base | Not every read/write/fetch permutation or noncanonical address |
| 035 | `paging_alias.sh`: production walk strips W from all low/high/user aliases, including huge leaves; `wx_alias.py`: hardware refuses kernel/user alias writes after RX publication | Owned kernel tree and inherited bootstrap user tree only, not arbitrary external roots or EPT |
| 036 | `ring3.c` loads while NX, seals all aliases, flushes, then publishes RX; tests cover this one-way publication | Reverse transition intentionally unsupported; no runtime JIT requirement inferred |
| 037 | Alias hardware fixture first loads code through writable identity mapping, seals + CR3-flushes, switches to shared inherited tree, then exact-address write faults | BSP only; case 3 probes immediately after sealing, before another root switch, isolating the seal flush |
| 038 | Existing framebuffer validation/output tests belong to display worker; paging preserves UC mapping contract | This component did not add framebuffer fuzzing |
| 039 | `high_memory.py`: 5/8 GiB actual high-frame allocation/read/write/free and inherited access | Final aggregate must rerun; no native hardware claim |
| 040 | `high_memory.py`: 256 MiB boot records no free high RAM and reaches idle | Final aggregate must rerun |
| 041 | `memory.sh`: descriptor crosses 4 GiB, full-width allocation/release; `paging_alias.sh`: low/high conversion boundary, >4 GiB huge alias sealing | Synthetic exact boundary |
| 042 | `memory.sh`: 128 TiB exclusive cap, physical-width admission; `paging_alias.sh`: max canonical conversion and rejection at cap | 128 TiB is a design/host-test limit, not a QEMU RAM-size result |
| 043 | `memory.sh`: sparse 1/32 TiB ranges and hole free rejection, reservations; alias walker leaves unmapped pages absent | A direct-map address formula never proves mapping validity |

Component commands:

- Source the available private dependency environment, then run
  `krnl/tests/memory.sh` and `krnl/tests/paging_alias.sh`.
- Run `python3 efi/tests/wx_alias.py` serially with other users of
  `efi/build/vars-run.fd`.
- Existing regressions: `python3 efi/tests/kernel_memory.py`,
  `bash efi/tests/ring3.sh`, `python3 efi/tests/high_memory.py`.

Component observations: seven PMM ASan/UBSan processes and the alias UBSan
process passed. The first two W^X QEMU cases passed with #PF 14/error 3/exact CR2.
A later IF-off context guard was compiled and host-tested; final aggregate
should rerun QEMU on that final source, including added case 3 that isolates
the seal flush before any later root switch.
