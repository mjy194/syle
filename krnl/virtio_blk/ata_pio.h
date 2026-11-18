#ifndef SYLE_DISK_ATA_PIO_H
#define SYLE_DISK_ATA_PIO_H
#include "rw_backend.h"
/* Nondefault nested-KVM fixture only: fixed PIIX3 IDE at00:05.0, primary
 * master, serial SYLE-PERSIST-0001, exactly64MiB. CPU PIO; bus-master DMA and
 * ATA interrupts disabled. The outer runner must admit this exact file/device
 * graph before allowing L1 to run. Never use against a physical host disk. */
bool syle_ata_bind(struct vrw_store*);
/* Flush and relinquish the synchronous PIO binding after guest quiescence.
 * Failure retains ownership; success is idempotent and permits a fresh bind. */
bool syle_ata_close(void);
#endif
