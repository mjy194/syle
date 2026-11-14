# Include AFTER firmware-ovmf-irq.mk; only observer output scheduling differs.
build/fq-firmware_ovmf_quiet_observe.o: ../krnl/firmware_ovmf_quiet_observe.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -DLRTC_FIRMWARE_COLD_BOOT -DSYLE_CACHE_KVM_FIXTURE -DSYLE_FIRMWARE_IRQ_FIXTURE -c $< -o $@
build/fq-firmware_observer_trace.o: ../krnl/firmware_observer_trace.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-ovmf-quiet.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) $(filter-out build/fc-firmware_ovmf_clock_observe.o build/fc-firmware_clock_apic_transaction.o build/fc-firmware_mmio_memory.o,$(FC_OBJS)) build/fq-firmware_ovmf_quiet_observe.o build/fk-firmware_cache_kvm_native.o build/fk-firmware_wbinvd.o build/fk-firmware_tpm_absent.o build/fk-firmware_tpm_transaction.o build/fk-firmware_vars_rom.o build/fk-firmware_dirty_transaction.o build/fk-firmware_dirty_cas.o build/fk-firmware_vars_dirty.o build/fk-firmware_vars_dirty_transaction.o build/fk-firmware_accessed_cas.o build/fk-firmware_apic_accessed.o build/fk-firmware_apic_accessed_transaction.o build/fk-firmware_apic_svr_dirty.o build/fk-firmware_apic_svr_dirty_transaction.o build/fk-firmware_timer_div2to1.o build/fk-firmware_clock_apic_transaction.o build/fo-rom.o $(IRQ_OBJS) build/fq-firmware_observer_trace.o build/fq-firmware_irq_lvt_same.o build/fi-de-firmware_mmio_memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-quiet
firmware-ovmf-quiet: build/krnl-ovmf-quiet.elf

build/fq-firmware_irq_lvt_same.o: ../krnl/firmware_irq_lvt_same.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
