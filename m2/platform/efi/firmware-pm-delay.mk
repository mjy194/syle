# Include after firmware-pause.mk and before firmware-ovmf-quiet.mk.
PM_DELAY_CAP ?= 6144
FQ_KERNEL := build/krnl-pm-delay-$(PM_DELAY_CAP).elf
FQ_OBSERVER := build/fpm-delay-$(PM_DELAY_CAP)-observer.o
$(FQ_OBSERVER): KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE -DFPM_DELAY_EXIT_CAP=$(PM_DELAY_CAP)
$(FQ_KERNEL): build/fpause-model.o build/fq-firmware_code_rom.o
.PHONY: firmware-pm-delay
firmware-pm-delay: $(FQ_KERNEL)
