# Syle EFI · Stage 0

从零自研 L0 项目的第一步：先打通 **QEMU → OVMF → EFI 应用 → ExitBootServices → 独立日志/断点**。
本项目尚不是可运行客体的 Hypervisor。gnu-efi 仅用于 EFI ABI/重定位，不是 Hypervisor 基座。

## 阶段边界

已实现的源码路径：

1. `EFI/BOOT/BOOTX64.EFI` 默认启动，COM1 115200 串口输出。
2. 打印实际装载基址、后续断点地址，以及 CPUID VMX 位。
3. 获取内存映射，预留描述符余量；以最新 map key 调用 `ExitBootServices`。
4. 退出固件 Boot Services 后直接访问 UART；禁用中断并停在可调试的 `stage0_post_ebs`。
5. 独立冒烟构建使用 QEMU debug-exit 结束测试，不用于正常运行。

**未实现**：自己的页表、正常执行独立栈、内存分配器、VMXON/VMCS、EPT、AP 启动、Windows 引导、嵌套 VMX、Hyper-V 兼容性。已补自有 GDT/TSS/IDT、静态 IST 异常栈与基本 panic。
退出 Boot Services 后当前仍沿用固件建立的执行环境，不能把这个停机点称为完成 Hypervisor 初始化。
保留 EFI 内存映射所在内存，但本阶段尚未对它建立分配器或回收内存。

## 构建

从项目根目录先执行 `cd efi`；下文命令均在 `syle/efi/` 下运行。

项目名为 **Syle**。`stage0` 构建产物名、函数名和 `STAGE0.*` 日志标记继续表示当前启动阶段，保留以兼容调试脚本和已有验证记录。

依赖：x86-64 Linux、GCC、GNU binutils、make、gnu-efi；运行还需 QEMU x86、OVMF，调试需 GDB，测试需 Python 3/timeout。
在 Debian 已有工具的机器上：

```sh
make
make smoke
```

gnu-efi 在用户目录解包时：

```sh
export EFI_PREFIX=/path/to/unpacked/usr
make
```

生成 `build/esp/EFI/BOOT/BOOTX64.EFI` 和带 DWARF 符号的 `build/stage0.so`。
不要用剥离后的 EFI 文件作为源码调试符号来源。

## 运行与验证

```sh
# 如发行版默认路径不同，请设置成一对匹配的非 Secure Boot OVMF 文件：
export OVMF_CODE=/path/to/OVMF_CODE_4M.fd
export OVMF_VARS=/path/to/OVMF_VARS_4M.fd
# 如非系统安装，请令 QEMU 指向能够找到动态库及固件资源的启动包装脚本：
export QEMU=/path/to/qemu-system-x86_64
scripts/run.sh run
# 正常版本在打印 STAGE0.POST_EBS 后 HLT，手动 Ctrl-C 结束。
tests/smoke.sh
```

测试不仅检查日志，还要求进程 exit code 为 33、关键标记按序出现且没有 ERROR。
超时/固件启动失败/崩溃都不会被当成通过。测试日志保存在 `build/smoke.log`。
脚本固定单 CPU、TCG、无网络；不会使用或修改宿主机虚拟化/安全设置。
`build/vars-*.fd` 每次由只读模板复制，避免历史 UEFI 变量影响测试。
不要同时启动相同模式的两个实例，因为它们共享变量文件。

## GDB

```sh
scripts/run.sh debug  # CPU 从 reset vector 暂停，GDB 仅监听 127.0.0.1:1234
# 另一个终端：
gdb -x scripts/debug.gdb
```

从串口先取得一次 `IMAGE.BASE`，调试启动使用相同固件/内存/磁盘配置，然后：

```gdb
add-symbol-file build/stage0.so -o 0x实际装载基址
hbreak efi_main
continue
hbreak stage0_post_ebs
continue
list
info registers
x/10i $pc
```

这是固件装载的可重定位镜像，不能假定永久固定地址；若下一次装载地址不同应以当次日志为准重新加载符号。
在 EFI 尚未装载时使用硬件断点，不把软件断点提前写进未来要被固件覆盖的内存。

## 下一阶段建议

再补正常执行独立栈、页表/内存管理与更完整异常验证，然后实现最小 VMX bring-up。
TCG 能验证本阶段的 UEFI 与调试链，**不能据此验证 Intel VMX 或嵌套 Hyper-V**。
VMX 阶段需可用且暴露 VMX 的硬件/KVM 环境，届时先核查能力与 MSR，不可把 CPUID 一个标志当成 VMXON 成功。

## 本次云端验证结果（2026-11-07）

当前环境用官方 Debian 包解包到 `/workspace/shared/qemu-deps/root`，未系统安装、未提权。

```sh
source scripts/cloud-env.sh
make all smoke
tests/smoke.sh
python3 tests/debugger.py
```

- GCC 14.2.0 / GNU ld 2.44，编译 `-Wall -Wextra -Werror` 通过。
- QEMU 10.0.13，TCG + OVMF 实际启动成功。
- 冒烟：UEFI_ENTRY → EXIT_BOOT_SERVICES_BEGIN → POST_EBS → SMOKE_PASS；退出码 33，检查通过。
- GDB 16.3 实际停在 `efi_main` 和 `stage0_post_ebs`，可看源码、RIP/RSP。
- 本次镜像基址 `0x0e370000`；正常构建 post-EBS 断点 `0x0e3730c9`，不视为跨环境常量。
- 客体 CPUID `VMX=0`；宿主没有 `/dev/kvm`、没有暴露 VMX/SVM。未尝试 VMXON，未运行 Hyper-V。
- 相关原始输出：`build/build.log`、`build/smoke.log`、`build/gdb.log`、`build/debug-serial.log`。

用户后续选用其他环境；本机仅继续可迁移的 QEMU 软件原型，不扩展硬件虚拟化阶段。


## 屏幕优先 + 串口同步 / 真实异常入口

新增 `src/console.c` 内置自绘 5×7 英文位图字体（2 倍绘制，小写显示成大写），
初始化时取得 GOP 的 32 位 RGB/BGR framebuffer 并复制地址、尺寸、stride。
后续包括 panic 均直接写显存，不调用 GOP Blt/EFI 服务，不动态分配。
提供换行/滚屏、固定暗红 panic 背景。PixelBitMask/BltOnly 或非法 framebuffer 将禁用屏幕，保留串口路径。

串口是可选的 QEMU/传统 COM1 原型。检查 LSR/暂存寄存器，不存在时关闭 UART 输出；
写入有固定轮询上限。**这不证明物理电脑有 UART，也不自动发现 ACPI SPCR/PCI UART。**
屏幕优先写出，每字符再尝试串口；不依赖串口成功才能显示。

退出 Boot Services 后安装自有 GDT、TSS、32 个 CPU 异常入口与 IDT：
- 32 KiB 静态 IST 异常栈，另预留 16 KiB double-fault IST 栈。
- 汇编归一化有/无 error-code 的异常帧，保存 15 个通用寄存器。
- 现场输出 vector/error/RIP/CS/RFLAGS/RSP/SS/CR2 与通用寄存器。
- `panic(message)` 支持主动停止；嵌套 panic 直接停机，避免递归输出。
- 本阶段不恢复故障执行、不保存 SIMD/XSAVE，不处理正常设备中断；单 CPU，IF 关闭。
- CR2 只有页故障时有本次诊断意义，其余异常可能是旧值。

```sh
source scripts/cloud-env.sh
make all smoke faults
tests/smoke.sh
python3 tests/display_faults.py
python3 tests/debugger.py
NO_GOP=1 tests/smoke.sh
# 支持 GUI 的机器可显示实际 QEMU 窗口：
DISPLAY_BACKEND=gtk scripts/run.sh ud
# 关闭串口仅用屏幕：
NO_UART=1 DISPLAY_BACKEND=gtk scripts/run.sh ud
```

默认无 GUI，测试通过只监听 localhost 的 QMP 截取屏幕。
`build/run.png`、`build/ud.png`、`build/pf.png`、`build/ud-no-uart.png` 是本次截图，
对应原始截图为 PPM。`ud` 是真实 `ud2`，`pf` 是真实访问未映射地址，
不是手动打印假现场。测试检查 vector6/vector14、PF error2 和 CR2。
无串口案例另经截图肉眼确认红底 panic 可读。QEMU 保持停机，由测试清理，不自动重启。

**适用边界**：本次只证明 QEMU/OVMF 在 ExitBootServices 后仍可直接写 GOP framebuffer。
不证明 Windows 显卡驱动接管、切模式、关闭固件映射后仍能使用该地址。
实际硬件若 framebuffer 不可写或映射损坏，panic 屏幕也可能失败；嵌套 panic 选择停机而非冒险恢复。
没有验证 double-fault/栈损坏、多核并发、NMI重入、所有32向量或物理机兼容性。

## EFI → krnl 交接（v1）

成功 ExitBootServices 后，通过版本化 `syle_boot_info` 进入
`../krnl/main.c:syle_main`。仍构建为一个 EFI 镜像；旧 GDB 断点及测试分支保留。
交接信息和内存保留限制详见 [krnl 文档](../krnl/README.md)。
