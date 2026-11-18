# Include after firmware-pause.mk (and firmware-pm-delay.mk for its snapshot),
# before firmware-ovmf-quiet.mk. Dedicated opt-in observer and kernel.
FQ_KERNEL := build/krnl-firmware-halt.elf
FQ_OBSERVER := build/fhalt-observer.o
build/fhalt-observer.o: KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE -DSYLE_FIRMWARE_HALT
build/fhalt-observer.o: ../krnl/firmware_halt_observe.inc
build/fhalt-model.o: ../krnl/firmware_halt.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-halt.elf: build/fhalt-model.o build/fpause-model.o build/fq-firmware_code_rom.o
.PHONY: firmware-halt
firmware-halt: build/krnl-firmware-halt.elf
