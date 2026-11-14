# Explicit isolated IRQ experiment; include after cache-kvm mk. Never default.
IRQ_MODS=linux_external_irq firmware_timer_irq_owner firmware_timer_irq_import firmware_timer_irq_route firmware_timer_irq_session firmware_timer_irq_session_import firmware_irq_receipt firmware_irq_native_evidence firmware_timer_irq_delivery firmware_irq_window firmware_irq_apic firmware_irq_cr8 firmware_irq_cr8_write firmware_cpu_mode_de firmware_active_cpu_de firmware_debug_read
IRQ_OBJS=$(addprefix build/fi-,$(addsuffix .o,$(IRQ_MODS)))
build/fi-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fi-firmware_ovmf_irq_observe.o: ../krnl/firmware_ovmf_irq_observe.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -DLRTC_FIRMWARE_COLD_BOOT -DSYLE_CACHE_KVM_FIXTURE -DSYLE_FIRMWARE_IRQ_FIXTURE -c $< -o $@
# Explicit dependency snapshot; keep source profile isolated.
build/krnl-ovmf-irq.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) $(filter-out build/fc-firmware_ovmf_clock_observe.o build/fc-firmware_clock_apic_transaction.o build/fc-firmware_mmio_memory.o,$(FC_OBJS)) build/fi-firmware_ovmf_irq_observe.o build/fk-firmware_cache_kvm_native.o build/fk-firmware_wbinvd.o build/fk-firmware_tpm_absent.o build/fk-firmware_tpm_transaction.o build/fk-firmware_vars_rom.o build/fk-firmware_dirty_transaction.o build/fk-firmware_dirty_cas.o build/fk-firmware_vars_dirty.o build/fk-firmware_vars_dirty_transaction.o build/fk-firmware_accessed_cas.o build/fk-firmware_apic_accessed.o build/fk-firmware_apic_accessed_transaction.o build/fk-firmware_apic_svr_dirty.o build/fk-firmware_apic_svr_dirty_transaction.o build/fk-firmware_timer_div2to1.o build/fk-firmware_clock_apic_transaction.o build/fo-rom.o $(IRQ_OBJS) build/fi-de-firmware_mmio_memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-irq
firmware-ovmf-irq: build/krnl-ovmf-irq.elf

build/fi-de-firmware_mmio_memory.o: ../krnl/firmware_mmio_memory.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FMM_DE_CONFIG -c $< -o $@
