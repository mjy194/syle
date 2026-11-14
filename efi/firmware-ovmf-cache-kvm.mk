# Nondefault scoped QEMU/KVM fixture only; never included by default Makefile.
# Include after firmware-probe.mk, firmware-ovmf-observe.mk, firmware-ovmf-clock.mk.
# Building is NOT permission to run; outer VMCS01 interception must be proven.
build/fk-firmware_ovmf_cache_kvm_observe.o: ../krnl/firmware_ovmf_cache_kvm_observe.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -DLRTC_FIRMWARE_COLD_BOOT -DSYLE_CACHE_KVM_FIXTURE -c $< -o $@
build/fk-firmware_cache_kvm_native.o: ../krnl/firmware_cache_kvm_native.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_CACHE_KVM_FIXTURE -c $< -o $@
build/fk-firmware_wbinvd.o: ../krnl/firmware_wbinvd.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_tpm_absent.o: ../krnl/firmware_tpm_absent.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_tpm_transaction.o: ../krnl/firmware_tpm_transaction.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_vars_rom.o: ../krnl/firmware_vars_rom.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_dirty_transaction.o: ../krnl/firmware_dirty_transaction.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_dirty_cas.o: ../krnl/firmware_dirty_cas.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_vars_dirty.o: ../krnl/firmware_vars_dirty.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_vars_dirty_transaction.o: ../krnl/firmware_vars_dirty_transaction.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_accessed_cas.o: ../krnl/firmware_accessed_cas.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_apic_accessed.o: ../krnl/firmware_apic_accessed.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_apic_accessed_transaction.o: ../krnl/firmware_apic_accessed_transaction.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_apic_svr_dirty.o: ../krnl/firmware_apic_svr_dirty.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_apic_svr_dirty_transaction.o: ../krnl/firmware_apic_svr_dirty_transaction.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_timer_div2to1.o: ../krnl/firmware_timer_div2to1.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fk-firmware_clock_apic_transaction.o: ../krnl/firmware_clock_apic_transaction.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_EXPERIMENTAL_DIV2_TO_1 -c $< -o $@
build/krnl-ovmf-cache-kvm.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) $(filter-out build/fc-firmware_ovmf_clock_observe.o build/fc-firmware_clock_apic_transaction.o,$(FC_OBJS)) build/fk-firmware_ovmf_cache_kvm_observe.o build/fk-firmware_cache_kvm_native.o build/fk-firmware_wbinvd.o build/fk-firmware_tpm_absent.o build/fk-firmware_tpm_transaction.o build/fk-firmware_vars_rom.o build/fk-firmware_dirty_transaction.o build/fk-firmware_dirty_cas.o build/fk-firmware_vars_dirty.o build/fk-firmware_vars_dirty_transaction.o build/fk-firmware_accessed_cas.o build/fk-firmware_apic_accessed.o build/fk-firmware_apic_accessed_transaction.o build/fk-firmware_apic_svr_dirty.o build/fk-firmware_apic_svr_dirty_transaction.o build/fk-firmware_timer_div2to1.o build/fk-firmware_clock_apic_transaction.o build/fo-rom.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-cache-kvm
firmware-ovmf-cache-kvm: build/krnl-ovmf-cache-kvm.elf
