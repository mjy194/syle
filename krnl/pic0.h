#ifndef SYLE_PIC0_H
#define SYLE_PIC0_H
#include <stdint.h>
#include <stdbool.h>
struct pic0 {
 uint64_t domain,seen,serial,reserved,reserved_seen,completed;
 uint8_t imr,vector,phase;
 bool mask_pending,initialized,aeoi,read_isr,irr,isr;
};
bool pic0_init(struct pic0*,uint64_t domain,uint64_t initial_edge_sequence);
bool pic0_edges(struct pic0*,uint64_t domain,uint64_t cumulative);
bool pic0_access(struct pic0*,uint16_t port,bool write,uint8_t*);
bool pic0_peek(const struct pic0*,uint8_t*);
bool pic0_reserve(struct pic0*,uint64_t* token,uint8_t* vector);
bool pic0_accept(struct pic0*,uint64_t domain,uint64_t token,uint8_t vector);
bool pic0_cancel(struct pic0*,uint64_t domain,uint64_t token,uint8_t vector);
#endif
