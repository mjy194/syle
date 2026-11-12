# Syle

从 EFI 启动、从零构建的裸机 Hypervisor 项目。当前已实现第一阶段最小启动闭环：

```text
UEFI → efi 固件适配 → boot 配置/菜单 → ExitBootServices
     → krnl → 自有页表/栈 → ELF 管理程序（Ring 3）→ 系统调用退出 → 内核 idle
```

新增只读 VT-x 能力检查，以及可选 VMXON/VMXOFF 和最小 VMCS/VMLAUNCH/VMCALL 诊断构建。
当前环境无 VMX，仅验证了不可用分支、编译和策略单元测试；尚未实测 VMX root 或 guest/VM exit。
guest 诊断共享内核地址空间，不提供隔离；Windows 引导和嵌套虚拟化仍未实现。详见 [VMX 状态](krnl/VMX.md)。

## 目录和职责

```text
syle/
├── efi/                   # UEFI 协议适配、目前共用的早期诊断、构建/QEMU
├── boot/                  # 独立于 EFI 类型的配置解析、菜单和启动策略
├── include/syle/
│   ├── boot.h             # V3 稳定数据交接（不含服务指针）
│   └── boot_platform.h    # 仅 Boot Services 存活期间使用的回调
├── krnl/                  # 内核入口、异常、地址空间与用户态切换、ELF 加载
└── user/mgmt/             # 独立 ELF 管理程序源码与链接脚本
```

`efi` 和 `boot` 组成 EFI 启动器；`krnl` 是独立的静态 ELF64 内核文件，
不再链接进 EFI 镜像。boot 校验 ELF、通过 EFI 申请指定物理内存并加载段，
退出 Boot Services 后跳到独立 `kernel_entry`，立即使用内核自己的启动栈。
管理程序也是独立 ELF，由 EFI 从启动设备读取、krnl 校验并装入用户页。

当前内核固定链接/装载于 **64 MiB (`0x04000000`)**，最大跨度 8 MiB。
若该范围被固件占用，明确失败，不覆盖保留区；没有重定位、KASLR 或任意地址加载。
默认 QEMU 曾验证 16 MiB 被 BootServicesData 占用并被正确拒绝，因此使用 64 MiB。

ESP 布局：

```text
EFI/BOOT/BOOTX64.EFI
syle/boot.cfg
syle/krnl.elf
syle/mgmt.elf
```

`boot.cfg` 可配置 `default=normal|diagnostic`、`timeout=0..30`、`title=...`。
键盘 `1` 启动管理程序，`2` 诊断停机，Enter 选择默认项。无输入接口自动默认；
缺配置使用默认值，配置错误则诊断停机。菜单属于启动前 `boot`，不是运行时管理 UI。

## 构建与运行

```sh
cd efi
source scripts/cloud-env.sh  # 仅当前云端已解包依赖；其他环境见 EFI 文档
make all smoke faults ring3
scripts/run.sh              # 默认菜单，3 秒后启动 Ring 3 管理程序
```

修改源配置 `boot/boot.cfg` 后重新 `make all`，会更新默认 ESP 的配置。
镜像通过 QEMU FAT 目录挂载；没有更改主机引导项、磁盘或 Secure Boot 设置。

## 验证

```sh
krnl/tests/memory.sh
boot/tests/policy.sh
boot/tests/kernel_elf.sh "$PWD/efi/build/krnl.elf"
# ELF 加载器宿主测试
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  krnl/user_elf.c krnl/tests/user_elf.c -o /tmp/syle-elf-test
ASAN_OPTIONS=detect_leaks=0 /tmp/syle-elf-test efi/build/mgmt-runtime.elf
# 以下在 efi/ 中、source scripts/cloud-env.sh 后执行
python3 tests/boot_chain.py
python3 tests/kernel_memory.py
tests/smoke.sh
tests/ring3.sh
python3 tests/debugger.py
python3 tests/display_faults.py
tests/console_memory.sh
```

验证范围是 QEMU TCG + OVMF，不代表真机、VMX、GPU 接管或 Windows 已验证。
程序退出后内核停在 idle，不代表已经有调度器。当前无抢占、堆分配器、进程恢复、
运行时文件系统、交互 shell 或管理 VM 的接口。内核现在有保守物理页分配器和早期 4 KiB 页表，
text RX、rodata RO/NX、data/stack RW/NX、空页不可访问，用户态继承这些 supervisor
映射。仍不是生产级安全边界，未支持多核、动态映射回收或任意固件缓存策略。
不回收固件内存，保守保留原映像及启动分配；管理程序仅支持明确受限的 ELF ABI。

详情：[boot](boot/README.md)、[用户态 ELF](krnl/USER_ELF.md)、
[诊断 Ring 3](krnl/RING3.md)、[EFI 验证记录](efi/VERIFICATION.md)。
