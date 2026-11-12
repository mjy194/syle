#ifndef PIC_MASTER_04_GLUE_H
#define PIC_MASTER_04_GLUE_H
#include "pic_master_04.h"
#include "linux_external_irq.h"
enum pmg_before_result {PMG_ERROR=-1,PMG_READY=0,PMG_DEFER_WINDOW=1};
enum pmg_outcome {PMG_NO_ENTRY,PMG_EXIT,PMG_UNKNOWN};
struct pmg {uint64_t domain;struct lei_irq event;struct pm04_ticket ticket;bool busy,failed;};
/* Caller MUST authenticate all pending PF/GP/BP owners, error/length metadata
 * and vectoring before calling. Snapshot flags alone are not ownership proof.
 * Current master is explicit (may be a staged whole-domain copy). */
bool pmg_init(struct pmg*,const struct pic_master_04*);
enum pmg_before_result pmg_before(struct pmg*,struct pic_master_04*,const struct lei_snapshot*,void*,lei_write);
bool pmg_after(struct pmg*,struct pic_master_04*,enum pmg_outcome,uint32_t reason,uint32_t vectoring,void*,lei_write);
#endif
