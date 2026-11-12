#ifndef PIC0_GLUE_H
#define PIC0_GLUE_H
#include "pic0.h"
#include "linux_external_irq.h"
enum pgg_before_result { PGG_ERROR=-1, PGG_READY=0, PGG_DEFER_WINDOW=1 };
enum pgg_outcome {PGG_NO_ENTRY,PGG_EXIT,PGG_UNKNOWN};
struct pgg {struct pic0 *pic;uint64_t domain;struct lei_irq event;bool busy,failed;};
/* Fresh object/guest-domain only, never discard an in-flight token via init. */
bool pgg_init(struct pgg*,struct pic0*);
enum pgg_before_result pgg_before(struct pgg*,const struct lei_snapshot*,void*,lei_write);
bool pgg_after(struct pgg*,enum pgg_outcome,uint32_t reason,uint32_t vectoring,void*,lei_write);
#endif
