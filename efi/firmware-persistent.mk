# After firmware-disk.mk, in a fresh persistent-progress/prepare.py source.
# The prepared frozen observer supplies this native profile; do not mix object
# directories with the readonly target. Explicit PIO backing + guest IRQ5.
build/fdisk-observer.o: KFLAGS += -DSYLE_FIRMWARE_PERSISTENT
build/fdisk-observer.o: ../krnl/firmware_persistent_observe.inc
build/fdisk-rw-backend.o: ../krnl/virtio_blk/rw_backend.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-rw-transport.o: ../krnl/virtio_blk/rw_transport.c | build
	$(CC) $(KFLAGS) -I../krnl -c $< -o $@
build/fdisk-ata.o: ../krnl/virtio_blk/ata_pio.c | build
	$(CC) $(KFLAGS) -DSYLE_PERSISTENT_KVM_FIXTURE -I../krnl -c $< -o $@
build/fdisk-legacy-pic_master_04.o: KFLAGS += -DSYLE_PIC_IRQ5
build/krnl-firmware-disk.elf: build/fdisk-rw-backend.o build/fdisk-rw-transport.o build/fdisk-ata.o
