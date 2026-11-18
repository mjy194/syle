# Applied only in a fresh graphics source snapshot, after firmware-managed.mk.
build/fmg-observer.o build/fmg-main.o: KFLAGS += -DSYLE_FIRMWARE_GRAPHICS
build/fc-firmware_fwcfg_transaction.o build/fq-firmware_fwcfg_bootmenu.o: KFLAGS += -DFFC_RAMFB
build/fgfx-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/krnl-firmware-disk.elf: build/fgfx-firmware_ramfb.o build/fgfx-ramfb_config.o build/fgfx-firmware_display.o
