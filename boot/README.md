# Syle boot 层

`boot` 是启动策略层，不依赖 EFI 类型，不是用户态运行时菜单。EFI adapter
通过 `include/syle/boot_platform.h` 提供临时的平台操作，boot 解析配置、展示
菜单、确定启动模式，然后调用 adapter 的 `launch` 完成资源准备与内核交接。
当前可与 EFI/krnl 链接到同一个镜像中；独立源码层不等于独立可执行镜像。

平台接口只能在 firmware Boot Services 仍有效期间调用，不能保存在交接给
内核的 `boot_info` 中。`launch` 成功后不返回；ExitBootServices 后也不得
返回 boot、再次调用输入/延迟/文件读取等 firmware 操作。

## 配置

EFI adapter 决定 ESP 上的配置路径；源模板为 `boot/boot.cfg`：

```ini
default=normal
timeout=3
title=Syle
```

- 最多 4096 字节，ASCII 文本，允许 LF/CRLF、空行、行首 `#` 注释、键值两侧空白。
- `default`：`normal`（管理程序）或 `diagnostic`（内核诊断后停机）。
- `timeout`：0–30 秒；0 立即启动，不等待按键。
- `title`：1–80 个可打印 ASCII 字符。
- 字段可省略；重复/未知字段、非法值、NUL、超长文本一律拒绝，不部分应用。
- 文件不存在：默认 normal、3 秒、Syle。
- 读取错误或无效配置：明确打印错误，采用 diagnostic、0 秒，不启动管理程序。
  这是第一阶段的安全降级，不等同于配置成功。

菜单支持 `1` 选择管理程序，`2` 选择诊断，Enter 选择配置默认项。
在超时后自动选默认项；输入接口不可用时立即使用默认项。延迟采用平台提供的
100 ms 有界轮询，不需要 boot 自己实现中断或时钟驱动。模式选择不会表示
VMX、Windows 或其他 guest 已实现。

## Host 验证

```sh
boot/tests/policy.sh
```

覆盖配置正/负路径、全有或全无的解析、超时、数字选择、Enter、无输入、缺失/坏
配置、超大回调结果、launch 失败；ASan 与 UBSan 开启。LSan 在当前 ptrace
执行环境无法运行，因此关闭；本模块不动态分配内存。EFI/QEMU 端到端测试由
`efi` 层负责，host 测试本身不证明固件集成成功。

集成说明：正常镜像严格服从选择。`smoke`、`ud`、`pf`、`ring3-*` 是专用
测试镜像，编译时测试目标优先于菜单/配置，启动日志会明确打印
`BOOT.TEST_IMAGE_OVERRIDE`；不要把这些镜像当正常启动配置使用。

独立内核加载已接入：`kernel_elf.c` 在 boot 层负责严格 ELF 校验/清零/复制；
EFI 适配负责读 `syle/krnl.elf` 和向固件申请确切物理页。完成加载并取得最终
memory map 后执行 EBS，跳到 ELF 声明并校验过的入口。见 [KERNEL_ELF.md](KERNEL_ELF.md)。
