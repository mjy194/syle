# Include AFTER firmware-code-rom.mk and BEFORE firmware-ovmf-quiet.mk.
# Separate observer and ELF; the normal quiet and CODE-only targets stay intact.
FQ_KERNEL := build/krnl-firmware-pause.elf
FQ_OBSERVER := build/fpause-observer.o
build/fpause-observer.o: KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE
build/fpause-model.o: ../krnl/firmware_pause.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-pause.elf: build/fpause-model.o build/fq-firmware_code_rom.o
.PHONY: firmware-pause
firmware-pause: build/krnl-firmware-pause.elf
