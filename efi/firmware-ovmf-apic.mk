# Include after firmware-probe.mk and firmware-ovmf-observe.mk.
# Independent opt-in register-only active xAPIC probe; no timer/IRQ activation.
FA_MODS = firmware_ovmf_apic_observe firmware_active_cpu firmware_apic_guard firmware_apic_transaction firmware_mmio_memory guest_mmio_mov guest_apic lapic lapic_timer firmware_cpu_profile firmware_pci_probe linux_pic_mask
FA_OBJS = $(addprefix build/fa-,$(addsuffix .o,$(FA_MODS)))
build/fa-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -c $< -o $@
build/krnl-ovmf-apic.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) $(FA_OBJS) build/fo-rom.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-apic
firmware-ovmf-apic: build/krnl-ovmf-apic.elf
