#ifndef SYLE_VIRTIO_FIRMWARE_PCI_BUS_H
#define SYLE_VIRTIO_FIRMWARE_PCI_BUS_H
#include "transport.h"
#include "pci_config_projection.h"
#include "../../krnl/firmware_piix_pm.h"
/* Isolated HOST pilot: sole CF8 latch is board.pci.address. Preserves existing
 * restricted hostbridge/PIIX behavior; does not fabricate missing PIIX fn0.
 * Controller exclusively owns all callbacks and enclosing instruction commit.
 * Native mapping, IRQ, multi-head and complete PCI enumeration remain absent.
 * This board reserves D000-DFFF exclusively for this single 64-byte BAR. */
struct vfb {struct fpp_state board;struct vrt disk;struct vpc projection;bool projected,failed,busy;};
bool vfb_init(struct vfb*,struct vro_io,const uint8_t*,size_t,uint64_t,uint64_t,uint64_t);
/* Explicit diagnostic enumeration opt-in; legacy init keeps old board view. */
bool vfb_init_projected(struct vfb*,struct vro_io,const uint8_t*,size_t,uint64_t,uint64_t,uint64_t);
bool vfb_io(struct vfb*,uint64_t[15],uint64_t,unsigned,uint64_t,unsigned,uint64_t,uint64_t,const uint8_t*,unsigned,bool,void*,bool(*)(void*,uint32_t,uint64_t));
void vfb_abort(struct vfb*);
#endif
