# Explicit reconstructed single-entry target only; never part of all.
LR_INPUTS ?= ../../inputs
LR_BOOT_EFI ?= ../../../linux-first-exit/linux-first-exit-esp/EFI/BOOT/BOOTX64.EFI
LR_DEFS = -DMG_DIAGNOSTIC_GUEST_PF -DLINUX_FIRST_EXIT_RECOVERY -DLINUX_GUEST_PROBE -DSMOKE_TEST
LR_MODS = linux_boot linux_guest_memory linux_vcpu_prepare linux_vcpu_host linux_vcpu_backend linux_vcpu_state linux_vcpu_native linux_first_exit_recovery linux_recovery_mem
LR_OBJS = $(addprefix build/lr-,$(addsuffix .o,$(LR_MODS))) build/lr-native-asm.o build/lr-inputs.o build/lr-main.o build/lr-exceptions.o build/lr-exceptions-asm.o

build/lr-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(LR_DEFS) -c $< -o $@
build/lr-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(LR_DEFS) -c $< -o $@
build/lr-exceptions.o: src/exceptions.c | build
	$(CC) $(KFLAGS) $(LR_DEFS) -c $< -o $@
build/lr-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(LR_DEFS) -c $< -o $@
build/lr-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie $(LR_DEFS) -c $< -o $@
build/lr-inputs.S: $(LR_INPUTS)/bzImage $(LR_INPUTS)/initramfs.cpio | build
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global linux_kernel_start,linux_kernel_end,linux_initrd_start,linux_initrd_end' 'linux_kernel_start:' '.incbin "$(abspath $(LR_INPUTS)/bzImage)"' 'linux_kernel_end:' 'linux_initrd_start:' '.incbin "$(abspath $(LR_INPUTS)/initramfs.cpio)"' 'linux_initrd_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/lr-inputs.o: build/lr-inputs.S
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-linux-first-exit-recovery.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(LR_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
# Preserve exact verified EFI loader; only the kernel is rebuilt here.
build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI: $(LR_BOOT_EFI)
	printf '%s\n' 'a6464d8e9661d3c948e4951ee61a6e5541e74c0753bb50093fbd2c14d3fee074  $(LR_BOOT_EFI)' | sha256sum -c -
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-first-exit-recovery
linux-first-exit-recovery: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI build/linux-first-exit-recovery-esp/syle/krnl.elf

.PHONY: lr-check-inputs
lr-check-inputs:
	printf '%s\n' '3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6  $(LR_INPUTS)/bzImage' '7f71f66ed40ca0fa5e26e9dba8138672c148b0f720ed856a118b8955c36fa0a5  $(LR_INPUTS)/initramfs.cpio' | sha256sum -c -
build/lr-inputs.o: lr-check-inputs

# Separate bounded CR-only image; retained first-exit runner is untouched.
LC_DEFS = -DLINUX_CR_RECOVERY -DLINUX_GUEST_PROBE -DSMOKE_TEST
build/lc-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(LC_DEFS) -c $< -o $@
build/lc-runner.o: ../krnl/linux_cr_recovery.c | build
	$(CC) $(KFLAGS) $(LC_DEFS) -c $< -o $@
build/krnl-linux-cr-recovery.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lc-main.o build/lc-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-cr-recovery-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-cr-recovery
linux-cr-recovery: build/linux-cr-recovery-esp/EFI/BOOT/BOOTX64.EFI build/linux-cr-recovery-esp/syle/krnl.elf

# Strict fixed-image compressed-phase PF injection; separate diagnostic target.
LP_DEFS = -DLINUX_PF_RECOVERY -DLINUX_GUEST_PROBE -DSMOKE_TEST
build/lp-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/lp-runner.o: ../krnl/linux_pf_recovery.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-pf-recovery.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lp-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-pf-recovery-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-pf-recovery
linux-pf-recovery: build/linux-pf-recovery-esp/EFI/BOOT/BOOTX64.EFI build/linux-pf-recovery-esp/syle/krnl.elf

# v4 raw state diagnostics only; v3 runner and policy are preserved.
build/lpd4-runner.o: ../krnl/linux_pf_diag_v4.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-pf-diag-v4.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lpd4-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-pf-diag-v4-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-pf-diag-v4
linux-pf-diag-v4: build/linux-pf-diag-v4-esp/EFI/BOOT/BOOTX64.EFI build/linux-pf-diag-v4-esp/syle/krnl.elf

# Separate exact consumed-event v5 guard; v3/v4 keep their legacy guard object.
build/lpv5-runner.o: ../krnl/linux_pf_v5.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-pf-v5.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lpv5-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-pf-v5-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-pf-v5
linux-pf-v5: build/linux-pf-v5-esp/EFI/BOOT/BOOTX64.EFI build/linux-pf-v5-esp/syle/krnl.elf

# v6 adds only guest GS_BASE emulation; previous profiles remain untouched.
build/lgv6-runner.o: ../krnl/linux_gs_v6.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-gs-v6.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lgv6-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-gs-v6-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-gs-v6
linux-gs-v6: build/linux-gs-v6-esp/EFI/BOOT/BOOTX64.EFI build/linux-gs-v6-esp/syle/krnl.elf

# Conservative synthetic CPUID profile; host baseline gated before VMX.
build/lcpuv7-runner.o: ../krnl/linux_cpu_v7.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-cpu-v7.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lcpuv7-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-cpu-v7-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-cpu-v7
linux-cpu-v7: build/linux-cpu-v7-esp/EFI/BOOT/BOOTX64.EFI build/linux-cpu-v7-esp/syle/krnl.elf

# v8 changes only entry budget to 64; TSC and timer caps are unchanged.
build/lcpuv8-runner.o: ../krnl/linux_cpu_v8.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-cpu-v8.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lcpuv8-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-cpu-v8-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-cpu-v8
linux-cpu-v8: build/linux-cpu-v8-esp/EFI/BOOT/BOOTX64.EFI build/linux-cpu-v8-esp/syle/krnl.elf

# Fixed active-long-mode guest EFER only; previous profiles remain unchanged.
build/leferv9-runner.o: ../krnl/linux_efer_v9.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-efer-v9.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/leferv9-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o build/lr-linux_efer_commit.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-efer-v9-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-efer-v9
linux-efer-v9: build/linux-efer-v9-esp/EFI/BOOT/BOOTX64.EFI build/linux-efer-v9-esp/syle/krnl.elf

# Read-only owned guest RAM page-table diagnostics; unchanged v9 policy.
build/lpdv10-runner.o: ../krnl/linux_pf_diag_v10.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-pf-diag-v10.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lpdv10-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o build/lr-linux_efer_commit.o build/lr-linux_pt_diag.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-pf-diag-v10-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-pf-diag-v10
linux-pf-diag-v10: build/linux-pf-diag-v10-esp/EFI/BOOT/BOOTX64.EFI build/linux-pf-diag-v10-esp/syle/krnl.elf

# Read-only owned guest RAM page-table diagnostics; unchanged v9 policy.
build/lpdv11-runner.o: ../krnl/linux_pf_diag_v11.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-pf-diag-v11.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lpdv11-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o build/lr-linux_efer_commit.o build/lr-linux_pt_diag.o build/lr-linux_pt_permissions.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-pf-diag-v11-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-pf-diag-v11
linux-pf-diag-v11: build/linux-pf-diag-v11-esp/EFI/BOOT/BOOTX64.EFI build/linux-pf-diag-v11-esp/syle/krnl.elf

# Single fixed high-PF injection profile; preserves previous diagnostic targets.
build/lhpv12-runner.o: ../krnl/linux_high_pf_v12.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-high-pf-v12.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lhpv12-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o build/lr-linux_efer_commit.o build/lr-linux_pt_diag.o build/lr-linux_pt_permissions.o build/lr-linux_high_pf.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-high-pf-v12-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-high-pf-v12
linux-high-pf-v12: build/linux-high-pf-v12-esp/EFI/BOOT/BOOTX64.EFI build/linux-high-pf-v12-esp/syle/krnl.elf

# Exact NX data-mapping contract; host-only build, no new Linux verification.
build/lhpnx-runner.o: ../krnl/linux_high_pf_nx.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-high-pf-nx.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/lhpnx-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o build/lr-linux_efer_commit.o build/lr-linux_pt_diag.o build/lr-linux_pt_permissions.o build/lr-linux_high_pf.o build/lr-linux_high_pf_data.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-high-pf-nx-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-high-pf-nx
linux-high-pf-nx: build/linux-high-pf-nx-esp/EFI/BOOT/BOOTX64.EFI build/linux-high-pf-nx-esp/syle/krnl.elf

# Polling TX-only UART staged I/O integration; host-only build.
build/luarth-runner.o: ../krnl/linux_uart_host_only.c | build
	$(CC) $(KFLAGS) $(LP_DEFS) -c $< -o $@
build/krnl-linux-uart-host-only.elf: $(filter-out build/k-exceptions.o build/k-exceptions-asm.o,$(KCOMMON)) $(filter-out build/lr-main.o build/lr-linux_first_exit_recovery.o,$(LR_OBJS)) build/lp-main.o build/luarth-runner.o build/lr-linux_vcpu_cr.o build/lr-linux_vcpu_policy.o build/lr-linux_vcpu_event.o build/lr-linux_pf_guard_v5.o build/lr-linux_vcpu_msr.o build/lr-linux_gs_commit.o build/lr-linux_cpu_commit.o build/lr-linux_efer_commit.o build/lr-linux_pt_diag.o build/lr-linux_pt_permissions.o build/lr-linux_high_pf.o build/lr-linux_high_pf_data.o build/lr-linux_uart_io.o build/lr-guest_uart.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/linux-uart-host-only-esp/EFI/BOOT/BOOTX64.EFI: build/linux-first-exit-recovery-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
.PHONY: linux-uart-host-only
linux-uart-host-only: build/linux-uart-host-only-esp/EFI/BOOT/BOOTX64.EFI build/linux-uart-host-only-esp/syle/krnl.elf
