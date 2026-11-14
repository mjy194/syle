# Independent nondefault Hv-time profile. Include after firmware-probe.mk and
# firmware-ovmf-observe.mk. No change to register-only or Linux targets.
FC_MODS = firmware_lint0_quiet firmware_platform_id firmware_port92 firmware_cpuhp_absent firmware_cmos_memory firmware_fwcfg firmware_fwcfg_target firmware_fwcfg_transaction firmware_piix_pm firmware_pmtimer firmware_preemption_resume firmware_cmos linux_rtc firmware_ovmf_clock_observe firmware_active_cpu firmware_apic_guard firmware_clock_apic_transaction firmware_timer_100m firmware_timer_100m_mmio firmware_mmio_memory guest_mmio_mov guest_apic lapic lapic_timer firmware_cpu_profile firmware_pci_probe linux_pic_mask linux_host_clock_hv linux_host_clock_native firmware_clock_reference_native
FC_OBJS = $(addprefix build/fc-,$(addsuffix .o,$(FC_MODS)))
build/fc-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -DLRTC_FIRMWARE_COLD_BOOT -c $< -o $@
build/krnl-ovmf-clock.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) $(FC_OBJS) build/fo-rom.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-clock
firmware-ovmf-clock: build/krnl-ovmf-clock.elf
