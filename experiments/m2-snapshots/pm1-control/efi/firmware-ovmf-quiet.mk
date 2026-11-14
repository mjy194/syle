# Include AFTER firmware-ovmf-irq.mk; only observer output scheduling differs.
build/fq-firmware_ovmf_quiet_observe.o: ../krnl/firmware_ovmf_quiet_observe.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -DLRTC_FIRMWARE_COLD_BOOT -DSYLE_CACHE_KVM_FIXTURE -DSYLE_FIRMWARE_IRQ_FIXTURE -DSYLE_FPM_SCI_CONTROL -c $< -o $@
build/fq-firmware_observer_compact.o: ../krnl/firmware_observer_compact.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-ovmf-quiet.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) $(filter-out build/fc-firmware_ovmf_clock_observe.o build/fc-firmware_clock_apic_transaction.o build/fc-firmware_mmio_memory.o build/fc-firmware_fwcfg.o build/fc-firmware_pci_probe.o build/fc-firmware_piix_pm.o build/fc-firmware_pmtimer.o,$(FC_OBJS)) build/fq-firmware_ovmf_quiet_observe.o build/fq-firmware_piix_pm_sci.o build/fq-firmware_pmtimer_sci.o build/fk-firmware_cache_kvm_native.o build/fk-firmware_wbinvd.o build/fk-firmware_tpm_absent.o build/fk-firmware_tpm_transaction.o build/fk-firmware_vars_rom.o build/fk-firmware_dirty_transaction.o build/fk-firmware_dirty_cas.o build/fk-firmware_vars_dirty.o build/fk-firmware_vars_dirty_transaction.o build/fk-firmware_accessed_cas.o build/fk-firmware_apic_accessed.o build/fk-firmware_apic_accessed_transaction.o build/fk-firmware_apic_svr_dirty.o build/fk-firmware_apic_svr_dirty_transaction.o build/fk-firmware_timer_div2to1.o build/fk-firmware_clock_apic_transaction.o build/fo-rom.o $(IRQ_OBJS) build/fq-firmware_observer_compact.o build/fq-firmware_irq_lvt_same.o build/fq-firmware_cmos_index_read.o build/fq-firmware_cmos_rate_same.o build/fq-firmware_cmos_valid_write.o build/fq-firmware_cmos_alarm_read.o build/fq-firmware_fwcfg_bootmenu.o build/fq-firmware_tsc_read.o build/fq-firmware_active_cpu_tsc.o build/fq-firmware_observer_stack.o build/fq-firmware_pci_enum.o build/fi-de-firmware_mmio_memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-quiet
firmware-ovmf-quiet: build/krnl-ovmf-quiet.elf

build/fq-firmware_irq_lvt_same.o: ../krnl/firmware_irq_lvt_same.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

build/fq-firmware_cmos_index_read.o: ../krnl/firmware_cmos_index_read.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

build/fq-firmware_cmos_rate_same.o: ../krnl/firmware_cmos_rate_same.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

build/fq-firmware_cmos_valid_write.o: ../krnl/firmware_cmos_valid_write.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

build/fq-firmware_cmos_alarm_read.o: ../krnl/firmware_cmos_alarm_read.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

# Explicit absence of fw_cfg kernel blobs; default firmware model unchanged.
build/fq-firmware_fwcfg_bootmenu.o: ../krnl/firmware_fwcfg.c | build
	$(CC) $(KFLAGS) -I../krnl -DFFC_NO_KERNEL_BLOBS -DFFC_BOOT_MENU_DEFAULT -c $< -o $@

build/fq-firmware_tsc_read.o: ../krnl/firmware_tsc_read.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fq-firmware_active_cpu_tsc.o: ../krnl/firmware_active_cpu_tsc.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

build/fq-firmware_observer_stack.o: ../krnl/firmware_observer_stack.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@

# Synthetic host bridge complete readonly256B view; observation profile only.
build/fq-firmware_pci_enum.o: ../krnl/firmware_pci_probe.c | build
	$(CC) $(KFLAGS) -I../krnl -DFPI_HOSTBRIDGE_READONLY_CONFIG -DFPI_HOSTBRIDGE_ENUM_NO_RESOURCES -c $< -o $@

build/fq-firmware_piix_pm_sci.o: ../krnl/firmware_piix_pm.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FPM_SCI_CONTROL -c $< -o $@
build/fq-firmware_pmtimer_sci.o: ../krnl/firmware_pmtimer.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FPM_SCI_CONTROL -c $< -o $@
