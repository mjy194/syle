#ifndef SYLE_SMP_EPT_H
#define SYLE_SMP_EPT_H
#include <stdint.h>
#include <stdbool.h>
struct smp_ept {uint64_t table[4],page[10],eptp,replacement_page;};
/* BSP-only allocation/publication; keep alive until every CPU has left VMX. */
bool smp_ept_prepare(struct smp_ept *);
/* Prepare-only variant, before exposing fixture to any CPU. GPA7000 holds
 * {old_value, ready, stop}; running guest owns first two, BSP owns stop. */
bool smp_ept_prepare_running(struct smp_ept *);
/* false leaves *old unchanged when not ready or fixture is invalid. */
bool smp_ept_heartbeat(const struct smp_ept *,uint64_t *old);
bool smp_ept_stop(struct smp_ept *);
/* BSP-only, after both guests are stopped. Publishes only; caller must
 * arrange acknowledged local INVEPT on every CPU before reuse. Old backing
 * remains owned until release after all CPUs are out of VMX. Single replace. */
bool smp_ept_select_data(struct smp_ept *,unsigned index);
bool smp_ept_replace_data(struct smp_ept *);
void smp_ept_release(struct smp_ept *);
uint64_t smp_ept_stack_word(const struct smp_ept *,unsigned cpu);
#endif
