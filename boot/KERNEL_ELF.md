# 独立 krnl ELF 装载器契约

`kernel_elf.c` 不调用固件，不申请内存，不运行载入的程序。EFI adapter 先读取
`krnl.elf`，调用 `syle_kernel_elf_inspect`，按得到的物理范围申请保留页，再调用
`syle_kernel_elf_load`；仅在成功后才允许 ExitBootServices 并跳转入口。

- ELF64、小端、x86-64、ET_EXEC；不支持重定位或动态链接。
- 文件上限 5 MiB（容纳固定 4 MiB guest 固件及 ELF 开销），最多 16 个 program headers。
- PT_LOAD 的虚拟地址和物理地址必须一致，保留范围以 64 MiB 为基址，页对齐跨度
  不超过 8 MiB；全部段位于此范围。
- PT_LOAD 权限只接受 R、RX、RW；禁止 W+X，也禁止段共享同一物理页。
- 要求合法的文件边界、`filesz <= memsz`、power-of-two 段对齐及地址/文件偏移同余。
- 入口必须落在 executable 段的文件有效字节中，不能落在 BSS、数据段或未映射空洞。
- 可接受 PT_NULL 及一个 RW 的 PT_GNU_STACK。其余类型均拒绝，包括 PT_DYNAMIC、
  PT_INTERP 和 PT_PHDR。链接脚本需匹配这个有意受限的首阶段格式。
- load 在完整校验输入、输出容量及 source/destination 不重叠后才写输出；整个保留
  跨度清零，然后复制段内容。失败不会部分修改目标缓冲区或输出 plan。

**权限检查不等于建立页表权限。** 装载器记录/校验 ELF 权限；实际 MMU 映射权限
由 kernel 初始化负责。第一阶段 identity mapping 是否继续使用大页、是否落实
内核 W^X 等，须按 kernel 实现单独说明。

`inspect` 不证明物理地址可分配。EFI adapter 必须使用精确地址分配并核对结果；
如果 64 MiB 被占用，明确失败，不覆盖原有内存、不自行改址（ET_EXEC 不可重定位）。

Host tests:

```sh
boot/tests/kernel_elf.sh
boot/tests/kernel_elf.sh /absolute/path/to/krnl.elf
```

测试开 ASan/UBSan；LSan 因执行环境 ptrace 限制关闭。文件装载测试不替代 QEMU
真实跳转及 ExitBootServices 时序验证。
