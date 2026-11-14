#ifndef SYLE_FIRMWARE_TIMER_STAGE_H
#define SYLE_FIRMWARE_TIMER_STAGE_H
#include "lapic_timer.h"
/* Host-only staged prototype, NOT linked into either firmware observer.
 * domain is a caller-owned nonzero identity for monotonic synthetic ticks:
 * 1024 domain ticks = one undivided timer tick. It is NOT a frequency claim.
 * No IRQ sink exists. Due unmasked events stay owned by the unchanged timer;
 * Caller MUST persistently latch STOP_IRQ outside this staged device state,
 * then forbid resume and ALL further calls (including init) for this session.
 * This pure model cannot enforce session lifecycle or prevent callers from
 * supplying a stale pre-deadline timestamp after STOP. Init is fresh-session
 * only. Clock samples must be globally monotonic, including rejected calls.
 * Copy this state across the caller's hardware transaction, then publish. */
struct fts_state { struct lapic_timer timer; uint64_t domain; };
enum fts_result { FTS_INVALID, FTS_OK, FTS_STOP_VECTOR, FTS_STOP_IRQ };
bool fts_init(struct fts_state *, uint64_t domain, uint64_t now);
enum fts_result fts_poll(struct fts_state *, uint64_t domain, uint64_t now);
enum fts_result fts_mmio(struct fts_state *, uint64_t domain, uint64_t now,
                        uint64_t offset, unsigned width, bool write, uint32_t *value);
#endif
