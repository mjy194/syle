/* Separate writable KVM profile. The original boot payload supplies capacity
 * admission only; every sector comes from the independently attached backing
 * file through the guarded PIO store. There is no RAM-image fallback. */
#include "virtio_blk/rw_transport.h"
#include "virtio_blk/firmware_ram_adapter.h"
#include "virtio_blk/ata_pio.h"
static struct vrw_store persistent_store;
static bool persistent_irq(bool);
static bool persistent_init(struct vrwt*s,struct vro_io io,const uint8_t*seed,size_t n){
 if(!seed||n!=64u*1024u*1024u||!syle_ata_bind(&persistent_store))return false;
 return vrwt_init(s,io,persistent_store);
}
#define vrt vrwt
#define vrt_init persistent_init
#define vrt_config vrwt_config
#define vrt_io vrwt_io
#include "firmware_disk_observe.inc"
static bool persistent_irq(bool level){
 return disk_legacy_ready&&pm04_level5(&disk_legacy.master,disk_legacy.master.domain,level);
}
static bool persistent_reboot;
static bool persistent_reset(uint64_t rip,uint64_t q,unsigned len){
 uint8_t b[15];size_t n=io_instruction_fetch(rip,b);unsigned p=q>>16;
 if(!disk_pic_active||!irq_identity()||!fpp_encoding(q,len,b,n)||(q&7))return false;
 if(q&8){if(p!=0x64&&p!=0xcf9)return false;if(!irq_write(0,0x681e,rip+len))return false;native.gpr[0]&=~UINT64_C(255);return true;}
 if(
 !((p==0x64&&(uint8_t)native.gpr[0]==0xfe)||(p==0xcf9&&((uint8_t)native.gpr[0]&6)==6))||
 !persistent_store.flush(persistent_store.ctx))return false;
 persistent_reboot=true;console_sample_quiet=false;log_text("OVMF.PERSIST guest_reset_requested backing_flushed1\n");return true;
}
/* Type-1-only platform: CFB is the legacy mechanism-selection probe latch;
 * it does not alter the distinct CF8 address DWORD or expose conf2 cycles. */
static uint8_t persistent_cfb;
static bool persistent_cpu_config_probe(uint64_t g[15],uint64_t rip,uint64_t q,unsigned len){
 uint8_t b[15];size_t n=io_instruction_fetch(rip,b);
 if(!fpp_encoding(q,len,b,n)||(q&7)||(q>>16)!=0xcfb)return false;
 if(!(q&8)&&(uint8_t)g[0]>1)return false;
 if(!irq_write(0,0x681e,rip+len))return false;
 if(q&8)g[0]=(g[0]&~UINT64_C(255))|persistent_cfb;
 else persistent_cfb=(uint8_t)g[0];
 return true;
}
static bool persistent_config_probe(uint64_t rip,uint64_t q,unsigned len){
 return irq_identity()&&persistent_cpu_config_probe(native.gpr,rip,q,len);
}
