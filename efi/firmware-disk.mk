# After firmware-console.mk, before firmware-ovmf-quiet.mk.
FQ_KERNEL := build/krnl-firmware-disk.elf
FQ_OBSERVER := build/fdisk-observer.o
build/fdisk-observer.o: KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE -DSYLE_FIRMWARE_HALT -DSYLE_FIRMWARE_CONSOLE -DSYLE_FIRMWARE_DISK
build/fdisk-observer.o: ../krnl/firmware_disk_observe.inc ../krnl/firmware_console_observe.inc ../krnl/firmware_halt_observe.inc
build/fdisk-%.o: ../krnl/virtio_blk/%.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_PROBE -DSYLE_FIRMWARE_DISK -c $< -o $@
build/fdisk-loader.o: src/main.c | build
	$(CC) $(CFLAGS) -DSYLE_FIRMWARE_DISK -c $< -o $@
build/fdisk-loader.so: build/fdisk-loader.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/firmware-disk-BOOTX64.EFI: build/fdisk-loader.so
	$(OBJCOPY) $(SECTIONS) -O efi-app-x86_64 $< $@
FP_OBJS := $(filter-out build/fp-main.o,$(FP_OBJS)) build/fdisk-main.o
build/krnl-firmware-disk.elf: build/fdisk-backend.o build/fdisk-transport.o build/fdisk-firmware_ram_adapter.o build/fconsole-model.o build/fconsole-uart.o build/fhalt-model.o build/fpause-model.o build/fq-firmware_code_rom.o
.PHONY: firmware-disk
firmware-disk: build/krnl-firmware-disk.elf build/firmware-disk-BOOTX64.EFI

build/fdisk-lvt-mask.o: ../krnl/firmware_irq_lvt_mask.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-lvt-mask.o

build/fdisk-linux-gs.o: ../krnl/linux_gs_commit.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-linux-msr.o: ../krnl/linux_vcpu_msr.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-linux-gs.o build/fdisk-linux-msr.o

FQ_LINK_EXCLUDE := build/fp-firmware_cpu_mode.o build/fi-de-firmware_mmio_memory.o build/fq-firmware_active_cpu_tsc.o
build/fdisk-cpu.o: ../krnl/firmware_boot_cpu.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-mode.o: ../krnl/firmware_cpu_mode.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_BOOT_LINUX -I../krnl -c $< -o $@
build/fdisk-memory.o: ../krnl/firmware_mmio_memory.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_BOOT_LINUX -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-cpu.o build/fdisk-mode.o build/fdisk-memory.o

build/fdisk-active-tsc.o: ../krnl/firmware_active_cpu_tsc.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-active-tsc.o

build/fdisk-microcode.o: ../krnl/linux_microcode_model.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-cpu-commit.o: ../krnl/linux_cpu_commit.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-linux-policy.o: ../krnl/linux_vcpu_policy.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-msr-bank.o: ../krnl/linux_msr_bank.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-microcode.o build/fdisk-cpu-commit.o build/fdisk-linux-policy.o build/fdisk-msr-bank.o

# Preserve loader bounds; debugging sections are not guest runtime bytes.
FQ_POST_LINK := $(OBJCOPY) --strip-debug

build/fdisk-invlpg.o: ../krnl/firmware_boot_invlpg.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-invlpg.o

build/fdisk-pit2.o: ../krnl/linux_pit2.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-pit2.o

build/fdisk-sysenter.o: ../krnl/linux_sysenter_msr.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-sysenter.o

build/fdisk-debug.o: ../krnl/firmware_boot_debug.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-debug.o

FDISK_LEGACY := linux_port80 linux_pic04_domain linux_pic04_runtime pic_master_04 pic_master_04_glue pit0 linux_uart_runtime linux_uart_model_io guest_wait
build/fdisk-legacy-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: $(addprefix build/fdisk-legacy-,$(addsuffix .o,$(FDISK_LEGACY)))

build/fdisk-gp.o: ../krnl/firmware_boot_gp.c | build
	$(CC) $(KFLAGS) -DSYLE_FIRMWARE_BOOT_LINUX -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fdisk-gp.o
