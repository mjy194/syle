# Additive supervisor-only reset/mode-switch probe; no default dependency.
FP_MODS = firmware_probe_mem firmware_layout firmware_memory firmware_vcpu_plan firmware_vcpu_host firmware_cpu_mode firmware_probe linux_vcpu_prepare linux_vcpu_host linux_vcpu_backend linux_vcpu_state linux_vcpu_native
FP_OBJS = $(addprefix build/fp-,$(addsuffix .o,$(FP_MODS))) build/fp-main.o build/fp-exceptions-asm.o build/fp-native-asm.o build/fp-rom.o
build/fp-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) -I../krnl -DSYLE_FIRMWARE_PROBE -c $< -o $@
build/fp-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_PROBE -c $< -o $@
build/fp-exceptions-asm.o: src/linux_exceptions.S firmware-probe.mk | build
	$(CC) -g -fno-pie -DLINUX_GUEST_PROBE -c $< -o $@
build/fp-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/fp-rom/reset.bin: ../guest/firmware-probe/reset.S ../guest/firmware-probe/link.ld ../guest/firmware-probe/build.sh | build
	sh ../guest/firmware-probe/build.sh "$(abspath build/fp-rom)"
build/fp-rom.S: build/fp-rom/reset.bin
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global firmware_probe_start,firmware_probe_end' 'firmware_probe_start:' '.incbin "$(abspath build/fp-rom/reset.bin)"' 'firmware_probe_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/fp-rom.o: build/fp-rom.S
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-firmware-probe.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(FP_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: firmware-probe
firmware-probe: build/krnl-firmware-probe.elf
