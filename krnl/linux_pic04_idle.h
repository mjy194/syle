#ifndef LINUX_PIC04_IDLE_H
#define LINUX_PIC04_IDLE_H
#include "linux_pic04_domain.h"
#include "linux_idle_pit.h"
enum lpi_reason {LPI_JUMP=1,LPI_ACTIVE,LPI_IF_CLEAR,LPI_BLOCKED,LPI_PENDING,LPI_ISR,LPI_MASKED,LPI_NO_TIMER,LPI_FUTURE_START};
/* Explicit discrete-event policy. UART character-time timeout is NOT a global
 * timed source. Integrating one requires earliest-deadline scheduling instead. */
bool lpi_plan(const struct lpd_domain*,uint64_t,const struct lip_idle*,uint64_t*,unsigned*);
#endif
