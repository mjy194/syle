> 高内存更新：低 4 GiB 使用精细权限映射；高物理内存通过 supervisor 高半区直映继承到用户根页表。启动对象/GOP 仍要求低地址，并在切换前检查。详见 MEMORY.md。

> 2026-11-08 内存阶段更新：下文旧的 EFI 内嵌、4 GiB RWX 大页描述均已过时。
> 现在 `krnl.elf` 独立加载，默认运行管理 ELF；所有路径先切到内核自有细粒度页表，
> Ring 3 根继承其 supervisor 映射。当前事实以 [README](README.md) 为准。

> 最新更新：`krnl` 已拆成独立 ELF，通过 boot 加载；详见 [项目 README](../README.md)。以下初始记录中的嵌入式内核描述已被替代。

> 更新（2026-11-08）：默认启动现已通过 boot 配置菜单进入独立 ELF 管理程序；下文保留初始诊断切片记录。当前路径与限制见 [USER_ELF.md](USER_ELF.md) 和 [项目 README](../README.md)。

# Ring3 bootstrap vertical slice

This is an **opt-in diagnostic** (`make -C efi ring3`), not completion of
Syle's first phase. The production design remains `efi → boot → krnl → user`;
`boot` and its configurable menu are not implemented yet. This test uses the
existing EFI → post-ExitBootServices `syle_main` diagnostic path. Normal,
smoke and fault-test builds do not enter userspace.

## Implemented boundary

- A separately assembled, linked position-independent `user/mgmt` program is
  embedded as a flat image. The kernel checks its size (1..4096 bytes), copies
  it into a reserved page and starts it at entry offset zero. This is **not**
  a filesystem/ELF loader or a loader for arbitrary untrusted executables.
- New CR3 root, kernel execution stack and separate syscall stack. Storage is
  statically reserved inside the EFI image; no memory-map reclamation occurs.
- Low 4 GiB is supervisor-only, with 2 MiB identity pages. The prototype requires
  its image and framebuffer below 4 GiB. This intentionally coarse kernel map
  is RWX; it is not kernel W^X hardening or a production MMIO/cache policy.
- At virtual `0x8000000000`: user RX code page, RW/NX data page, unmapped guard
  page, RW/NX stack page. All paging ancestors enforce U/S correctly. NXE and
  CR0.WP are enabled; inherited global translations are flushed (PGE disabled).
  Unused SYSCALL/SYSENTER entries are disabled rather than inheriting firmware
  entry state. LA57 and SMAP are explicitly unsupported rather than silently mis-handled.
- Ring3 code/data selectors and TSS.RSP0; `iretq` enters CPL3. A DPL3 `int 0x80`
  interrupt gate switches to ring0 stack, preserves registers, dispatches and
  returns via `iretq`. This is a syscall ABI, **not the SYSCALL instruction**.
- Minimal ABI: rax=1 bounded write(rdi pointer,rsi length ≤256), rax=2 online
  bootstrap CPU count (=1), rax=3 exit(status in rdi). Invalid/unknown calls
  return -1. Pointer checks cover only mapped user-readable ranges and avoid
  addition overflow. There is no concurrent unmap/copy race in this one-CPU demo.
- `mgmt` writes a message, checks rejection of a kernel-range pointer, queries
  CPU status and exits. It is a real CPL3 process, not a kernel menu renamed
  userspace. It does not yet manage VMs or launch guest firmware.

## Verification

```sh
source efi/scripts/cloud-env.sh
(cd efi && tests/ring3.sh)
```

Five fresh OVMF/QEMU TCG boots must exit with code 33:

| Case | Expected result |
|---|---|
| 0 | Four syscalls, including three returns to user; successful process exit |
| 1 | User read of kernel page rejected: #PF error 5 |
| 2 | User write to code rejected: #PF error 7 |
| 3 | User execute from stack rejected: #PF error 21 |
| 4 | User port I/O rejected: #GP error 0 |

The fault tests also check CPL3 and exact fault address where applicable; they
never accept an unrelated kernel exception as a pass. Logs: `efi/build/ring3-*.log`.
These are hardware-emulated architectural tests, not physical-hardware tests.

## Remaining first-phase work

Implement the actual boot layer/configuration and separate kernel/module loading;
general physical/page allocator and hardened per-process maps; scheduler/timer,
process creation/termination and fault recovery; user image format validation,
real management commands and stable syscall ABI. The diagnostic exits QEMU on
process exit/fault, keeps interrupts off and has no multitasking or availability
claims. FS/GS, SIMD context switching and multiple process address spaces are
not part of this prototype. No VMX, EPT, Windows boot, or nested Hyper-V is claimed.
