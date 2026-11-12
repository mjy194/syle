#ifndef PIC_MASTER_04_H
#define PIC_MASTER_04_H
#include <stdint.h>
#include <stdbool.h>
struct pm04_ticket {uint64_t domain,serial,generation;uint8_t source,vector;};
struct pic_master_04 {
 uint64_t domain,seen0,seen4,serial;
 struct pm04_ticket reserved,completed;
 uint8_t imr,vector,phase,irr,isr;
 bool level4,initialized,aeoi,read_isr,mask_pending,quarantined;
};
/* New domain only after complete guest teardown. No pointers/callbacks; caller
 * stages full domain. Source updates never mean guest interrupt delivery. */
bool pm04_init(struct pic_master_04*,uint64_t domain,uint64_t edges0,bool level4);
bool pm04_edges0(struct pic_master_04*,uint64_t domain,uint64_t cumulative);
bool pm04_level4(struct pic_master_04*,uint64_t domain,bool level);
bool pm04_access(struct pic_master_04*,uint16_t port,bool write,uint8_t*,bool slave_all_masked);
bool pm04_peek(const struct pic_master_04*,uint8_t *source,uint8_t *vector);
bool pm04_reserve(struct pic_master_04*,struct pm04_ticket*);
bool pm04_accept(struct pic_master_04*,const struct pm04_ticket*);
bool pm04_cancel(struct pic_master_04*,const struct pm04_ticket*);
bool pm04_quarantine(struct pic_master_04*,const struct pm04_ticket*);
#endif
