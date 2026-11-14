# Explicit NEW initramfs fixture. Does not alter historical managed input hashes.
# make -f Makefile -f managed-baseline.mk managed-baseline-probe \
#   MB_INPUTS=/absolute/input-directory MB_MANIFEST=/absolute/manifest.json
include managed-terminal.mk
MB_INPUTS ?= ../../baseline-inputs
MB_MANIFEST ?= $(MB_INPUTS)/manifest.json
MB_CHECK_SCRIPT ?= ../guest/linux-baseline/check-inputs.py
MB_EMBED_TAG ?= mb
MB_DEFS = -I../krnl -DSYLE_MANAGED_GUEST -DLINUX_GUEST_PROBE -DMG_LEGACY_PIC04
MB_OBJS = $(addprefix build/mb-,$(addsuffix .o,$(MG_MODS))) build/mb-native-asm.o build/$(MB_EMBED_TAG)-inputs.o build/mb-main.o build/mb-exceptions-asm.o
.PHONY: mb-check-inputs managed-baseline-probe managed-baseline-terminal
mb-check-inputs:
	python3 $(MB_CHECK_SCRIPT) --inputs "$(MB_INPUTS)" --manifest "$(MB_MANIFEST)"
build/mb-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(MB_DEFS) -c $< -o $@
build/mb-linux_guest_startup.o: ../krnl/linux_guest_startup.c mb-check-inputs | build
	$(CC) $(KFLAGS) $(MB_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/mb-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(MB_DEFS) -c $< -o $@
build/mb-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(MB_DEFS) -c $< -o $@
build/mb-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
# Phony validation is intentional: a new manifest/input path cannot reuse stale
# embedded bytes merely because its mtime precedes a previous build.
build/$(MB_EMBED_TAG)-inputs.S: mb-check-inputs | build
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global linux_kernel_start,linux_kernel_end,linux_initrd_start,linux_initrd_end' 'linux_kernel_start:' '.incbin "$(abspath $(MB_INPUTS)/bzImage)"' 'linux_kernel_end:' 'linux_initrd_start:' '.incbin "$(abspath $(MB_INPUTS)/initramfs.cpio)"' 'linux_initrd_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/$(MB_EMBED_TAG)-inputs.o: build/$(MB_EMBED_TAG)-inputs.S
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-baseline.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(MB_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/krnl-managed-baseline-terminal.elf: $(filter-out build/k-exceptions-asm.o build/k-runtime.o,$(KCOMMON)) $(filter-out build/k-ring3.o,$(KRUNTIME)) $(MB_OBJS) build/mt-runtime.o build/mt-ring3.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/mbp-main.o: ../user/mgmt/guest_shell_probe.c ../guest/linux-baseline/probe-config.h ../user/mgmt/guest_shell_probe.h | build
	$(CC) $(MT_FLAGS) -include ../guest/linux-baseline/probe-config.h -c $< -o $@
build/mbp-entry.o: ../user/mgmt/guest_shell_probe_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mbp-mem.o: ../user/mgmt/freestanding_mem.c | build
	$(CC) $(MT_FLAGS) -c $< -o $@
build/mgmt-guest-baseline-probe.elf: build/mbp-entry.o build/mbp-main.o build/mbp-mem.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
managed-baseline-probe: build/krnl-managed-baseline.elf build/mgmt-guest-baseline-probe.elf
managed-baseline-terminal: build/krnl-managed-baseline-terminal.elf build/mgmt-guest-terminal.elf
