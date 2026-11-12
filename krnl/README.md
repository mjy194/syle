# Syle krnl 当前实现

`krnl.elf` 是独立的固定地址 ELF，boot 从 ESP 读取并加载，EFI 不链接内核。
boot_info V3 定义于 `include/syle/boot.h`。

启动链：`EFI → boot 菜单/加载 → ExitBootServices → kernel_entry → 自有启动栈
→ GDT/TSS/IDT/IST → PMM → 早期自有 CR3 → 用户 ELF/Ring 3 或诊断停机`。

## 内存与权限

- 物理页分配器按 CPU 地址位数管理 EFI ConventionalMemory（当前直映上限 128 TiB），支持高于 4 GiB 的物理页，保留 loader、内核、
  boot_info、memory map、用户程序缓冲区、framebuffer 及所有固件类型内存。
- 初始化全量检查描述符，不接受重叠/溢出/不支持格式。支持单页分配、释放、
  所有权检查；无堆、无连续分配、无多核同步。详见 [MEMORY.md](MEMORY.md)。
- `paging.c` 用 PMM 分配页表页，4 KiB supervisor identity mappings：内核 text RX，
  rodata RO/NX，data/BSS/栈 RW/NX；空地址第一页未映射，设备空洞不做盲目映射。
- EFI RAM 描述符被映射以保留访问，但映射不等于允许分配。boot_info/map/user ELF
  缓冲区设为只读 NX。GOP framebuffer 单独映射为 RW/NX/UC。
- 需要 NX、4 级页表、标准 PAT 的 WB/UC 槽，拒绝 LA57/PCID/SMAP。启用 CR0.WP，
  清除继承全局 TLB，再切换 CR3。所有正常和诊断内核都走这个路径。
- 用户态根页表继承早期 supervisor 映射，不再创建覆盖整个 4 GiB 的 RWX 大页。
  用户 RX/RW-NX/guard 与 syscall 流程见 [USER_ELF.md](USER_ELF.md)。

## 生命周期边界

kernel_entry 已使用内核自己的 32 KiB 栈，但在完成上述初始化前仍短暂依赖
固件 identity paging。页表构建时使用固件对 ConventionalMemory 的可写 identity
映射；本原型并未支持任意 UEFI 页表布局。当前不回收任何固件分配，boot_info
中的 inherited_rsp/cr3 只是交接观测值，不表示它们仍是运行时栈和页表。

ACPI RSDP 是未验证指针，未解析 ACPI。GOP framebuffer 在驱动重新配置 GPU 后
不保证有效。UART 只实现 QEMU COM1。诊断代码仍暂复用 `efi/src` 的 freestanding
实现，内核不调用任何 EFI 服务或库。

尚无 VMX、VMCS/EPT、SMP、调度器、进程故障恢复、Windows 引导或完整管理界面。
当前内核固定 64 MiB，无重定位，所有验证为 QEMU TCG，不是硬件虚拟化验证。

## 验证

在 `efi` 下 source `scripts/cloud-env.sh` 后：

- `tests/smoke.sh`：独立内核、自有早期 CR3 与物理页分配闭环。
- `python3 tests/kernel_memory.py`：实际写 RX text、执行 NX 栈、读空地址触发预期 #PF。
- `tests/ring3.sh`：5 项 CPL3 进入/返回和权限异常。
- `python3 tests/boot_chain.py`：11 项菜单、装载、缺失/错误文件、无 GOP 和按键。
- `python3 tests/debugger.py`、`python3 tests/display_faults.py`：调试入口和异常显示。
- `../krnl/tests/memory.sh`：物理页管理 ASan/UBSan。
