#include "shootdown.h"
static bool member(const struct shootdown *s, unsigned cpu)
{
    return s && s->participants && s->generation && cpu<64 &&
           (s->participants & (UINT64_C(1)<<cpu));
}
bool shootdown_init(struct shootdown *s, uint64_t participants)
{
    if(!s || !participants)return false;
    *s=(struct shootdown){.phase=SHOOTDOWN_READY,.participants=participants,
                          .generation=1};
    return true;
}
bool shootdown_enter(struct shootdown *s, unsigned cpu, uint64_t generation)
{
    if(!member(s,cpu) || s->phase!=SHOOTDOWN_READY || generation!=s->generation ||
       (s->running & (UINT64_C(1)<<cpu)))return false;
    s->running |= UINT64_C(1)<<cpu;
    return true;
}
bool shootdown_leave(struct shootdown *s, unsigned cpu)
{
    if(!member(s,cpu) || s->phase!=SHOOTDOWN_READY ||
       !(s->running & (UINT64_C(1)<<cpu)))return false;
    s->running &= ~(UINT64_C(1)<<cpu);
    return true;
}
bool shootdown_begin(struct shootdown *s, uint64_t expected, uint64_t *next)
{
    if(!s || !next || !s->participants || !s->generation ||
       s->phase!=SHOOTDOWN_READY || s->running || expected!=s->generation ||
       s->generation==UINT64_MAX)return false;
    s->phase=SHOOTDOWN_PUBLISHING;
    s->acknowledged=0;
    ++s->generation;
    *next=s->generation;
    return true;
}
bool shootdown_published(struct shootdown *s)
{
    if(!s || !s->participants || !s->generation || s->running ||
       s->phase!=SHOOTDOWN_PUBLISHING)return false;
    s->phase=SHOOTDOWN_WAIT;
    return true;
}
bool shootdown_ack(struct shootdown *s, unsigned cpu, uint64_t generation)
{
    if(!member(s,cpu) || s->phase!=SHOOTDOWN_WAIT || generation!=s->generation ||
       (s->acknowledged & (UINT64_C(1)<<cpu)))return false;
    s->acknowledged |= UINT64_C(1)<<cpu;
    if(s->acknowledged==s->participants)s->phase=SHOOTDOWN_READY;
    return true;
}
void shootdown_fail(struct shootdown *s)
{
    if(s)s->phase=SHOOTDOWN_POISON;
}
