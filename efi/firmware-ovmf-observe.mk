# Include after Makefile and firmware-probe.mk. Tiny target stays unchanged.
OVMF_CODE ?=
OVMF_VARS ?=
build/fo-probe.o: ../krnl/firmware_ovmf_observe.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -c $< -o $@
.PHONY: fo-input-check
fo-input-check:
	@test -f "$(OVMF_CODE)" -a -f "$(OVMF_VARS)"
	@echo '624e06de18b4fa535e90db7160d00d3d07d206422b89999bf1e27d920264e4e0  $(OVMF_CODE)' | sha256sum -c -
	@echo '5d2ac383371b408398accee7ec27c8c09ea5b74a0de0ceea6513388b15be5d1e  $(OVMF_VARS)' | sha256sum -c -
build/fo-rom.S: fo-input-check | build
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global firmware_probe_start,firmware_probe_end,firmware_vars_start,firmware_vars_end' 'firmware_probe_start:' '.incbin "$(abspath $(OVMF_CODE))"' 'firmware_probe_end:' '.balign 16' 'firmware_vars_start:' '.incbin "$(abspath $(OVMF_VARS))"' 'firmware_vars_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/fo-rom.o: build/fo-rom.S
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-ovmf-observe.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(filter-out build/fp-firmware_probe.o build/fp-rom.o,$(FP_OBJS)) build/fo-probe.o build/fp-firmware_cpu_profile.o build/fp-firmware_pci_probe.o build/fp-linux_pic_mask.o build/fp-linux_disabled_apic.o build/fo-rom.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-ovmf-observe
firmware-ovmf-observe: build/krnl-ovmf-observe.elf
