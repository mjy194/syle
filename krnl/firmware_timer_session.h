#ifndef SYLE_FIRMWARE_TIMER_SESSION_H
#define SYLE_FIRMWARE_TIMER_SESSION_H
#include "firmware_timer_stage.h"
/* Independent host prototype. Fresh zero-initialized storage, private owner;
 * no raw copies, reinitialization, direct mutation or state rollback permitted.
 * Device snapshots may be staged; this outer lifecycle MUST NOT be rolled back
 * with a failed hardware transaction. It offers no IRQ delivery capability. */
struct fts_session {
    struct fts_state device;
    uint64_t observed_now;
    bool initialized, terminal;
    enum fts_result reason;
};
bool ftss_init(struct fts_session *,uint64_t domain,uint64_t now);
enum fts_result ftss_poll(struct fts_session *,uint64_t domain,uint64_t now);
enum fts_result ftss_mmio(struct fts_session *,uint64_t domain,uint64_t now,
                         uint64_t offset,unsigned width,bool write,uint32_t *value);
/* Hardware commit failure is terminal, even if device staging was successful. */
void ftss_abort(struct fts_session *);
#endif
