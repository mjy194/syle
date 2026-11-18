# Prepared, manifest-pinned firmware lifecycle profile only. Separate objects
# prevent reuse of readonly/persistent standalone observer or Ring3 builds.
FQ_OBSERVER := build/fmg-observer.o
build/fmg-observer.o: KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE -DSYLE_FIRMWARE_HALT -DSYLE_FIRMWARE_CONSOLE -DSYLE_FIRMWARE_DISK -DSYLE_FIRMWARE_PERSISTENT -DSYLE_FIRMWARE_MANAGED
build/fmg-observer.o: ../krnl/firmware_managed.inc ../krnl/firmware_persistent_observe.inc ../krnl/firmware_disk_observe.inc
FQ_LINK_EXCLUDE += build/fdisk-main.o build/k-ring3.o
build/fmg-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_PROBE -DSYLE_FIRMWARE_DISK -DSYLE_FIRMWARE_MANAGED -c $< -o $@
build/fmg-ring3.o: ../krnl/ring3.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_MANAGED -c $< -o $@
FMG_USER_FLAGS := -std=c11 -Os -Wall -Wextra -Werror -ffreestanding -fpie -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables
build/fmg-manager.o: ../user/mgmt/firmware_lifecycle.c ../user/mgmt/firmware_lifecycle.h | build
	$(CC) $(FMG_USER_FLAGS) -c $< -o $@
build/fmg-manager-entry.o: ../user/mgmt/firmware_lifecycle_entry.S | build
	$(CC) -fno-pie -c $< -o $@
build/firmware-mgmt.elf: build/fmg-manager-entry.o build/fmg-manager.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
build/fmg-image.S: build/firmware-mgmt.elf
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global firmware_mgmt_start,firmware_mgmt_end' 'firmware_mgmt_start:' '.incbin "$(abspath $<)"' 'firmware_mgmt_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/fmg-image.o: build/fmg-image.S
	$(CC) -c $< -o $@
build/krnl-firmware-disk.elf: build/fmg-main.o build/fmg-ring3.o build/fmg-image.o
