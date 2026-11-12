#ifndef PIC0_PORTS_H
#define PIC0_PORTS_H
#include "pic0.h"
#include "linux_pic_mask.h"
struct pcp {struct pic0 *master;struct lpic_state slave;bool busy,failed;};
bool pcp_init(struct pcp*,struct pic0*);
bool pcp_entry_ok(const struct pcp*,uint64_t flags);
bool pcp_io(struct pcp*,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,uint64_t flags,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
