# Include BEFORE firmware-ovmf-quiet.mk; distinct objects and ELF prevent
# a stale opt-in observer from being reused by the normal quiet target.
FQ_KERNEL := build/krnl-code-rom.elf
FQ_OBSERVER := build/fcode-observer.o
build/fcode-observer.o: KFLAGS += -DSYLE_FIRMWARE_CODE_ROM
build/fq-firmware_code_rom.o: ../krnl/firmware_code_rom.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-code-rom.elf: build/fq-firmware_code_rom.o
.PHONY: firmware-code-rom
firmware-code-rom: build/krnl-code-rom.elf
