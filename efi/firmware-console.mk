# Include after firmware-halt.mk, before firmware-ovmf-quiet.mk.
FQ_KERNEL := build/krnl-firmware-console.elf
FQ_OBSERVER := build/fconsole-observer.o
build/fconsole-observer.o: KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE -DSYLE_FIRMWARE_HALT -DSYLE_FIRMWARE_CONSOLE
build/fconsole-observer.o: ../krnl/firmware_console_observe.inc ../krnl/firmware_halt_observe.inc
build/fconsole-model.o: ../krnl/firmware_console.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fconsole-uart.o: ../krnl/linux_uart.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-console.elf: build/fconsole-model.o build/fconsole-uart.o build/fhalt-model.o build/fpause-model.o build/fq-firmware_code_rom.o
.PHONY: firmware-console
firmware-console: build/krnl-firmware-console.elf

KCOMMON := $(filter-out build/k-runtime.o,$(KCOMMON)) build/fconsole-runtime.o
build/fconsole-runtime.o: ../krnl/runtime.c | build
	$(CC) $(KFLAGS) -DSYLE_CONSOLE_SERIAL_DRAIN -c $< -o $@
