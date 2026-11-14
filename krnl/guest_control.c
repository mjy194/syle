#include "guest_control.h"
#include <limits.h>
#define ERR(e) (-(int64_t)SYLE_GUEST_##e)
void guest_control_init(struct guest_control *c, uint64_t manager,
                        const struct guest_backend *b, void *context) {
    *c = (struct guest_control){ .manager=manager, .context=context };
    if (b && b->prepare && b->start && b->stop && b->release && manager) {
        c->backend=*b;
        c->configured=true;
    }
}
/* On any uncertain backend state stop before freeing. Retrying stop/release
 * is part of the backend contract and must be idempotent. */
static int64_t dispose(struct guest_control *c) {
    if (c->state==SYLE_GUEST_RUNNING || c->state==SYLE_GUEST_QUARANTINED) {
        if (c->backend.stop(c->context)) {
            c->state=SYLE_GUEST_QUARANTINED;
            return ERR(EQUARANTINED);
        }
        c->state=SYLE_GUEST_STOPPED;
    }
    if (c->backend.release(c->context)) {
        c->state=SYLE_GUEST_QUARANTINED;
        return ERR(EQUARANTINED);
    }
    c->state=SYLE_GUEST_EMPTY;
    c->handle=0;
    return 0;
}
static int64_t call_locked(struct guest_control *c, uint64_t op,
                            uint64_t handle, uint64_t arg) {
    if (op < SYLE_GUEST_CREATE || op > SYLE_GUEST_CONSOLE_SPACE) return ERR(EINVAL);
    if (op==SYLE_GUEST_CREATE) {
        if (handle || arg!=SYLE_GUEST_BOOT_PROFILE) return ERR(EINVAL);
        if (!c->configured) return ERR(ENOTSUP);
        if (c->state!=SYLE_GUEST_EMPTY) return ERR(EBUSY);
        /* Never wrap/reuse handles during this controller lifetime. */
        if (c->generation==INT64_MAX) return ERR(EEXHAUSTED);
        c->handle=++c->generation;
        c->state=SYLE_GUEST_READY;
        if (c->backend.prepare(c->context,arg)) {
            c->state=SYLE_GUEST_QUARANTINED;
            /* No handle was returned. Explicit owner REAP can retry cleanup;
             * a new create remains blocked until cleanup really succeeds. */
            if (dispose(c)) return ERR(EQUARANTINED);
            return ERR(EBACKEND);
        }
        return (int64_t)c->handle;
    }
    if (op==SYLE_GUEST_CONSOLE_SEND ? arg>255 : arg!=0) return ERR(EINVAL);
    if (!c->configured) return ERR(ENOTSUP);
    if (op==SYLE_GUEST_REAP) {
        if (handle) return ERR(EINVAL);
        return c->state==SYLE_GUEST_EMPTY ? 0 : dispose(c);
    }
    /* Zero handle is not a wildcard: only owner REAP or owner exit can recover a failed
     * create whose handle was never returned. */
    if (!handle || handle!=c->handle || c->state==SYLE_GUEST_EMPTY)
        return ERR(ENOENT);
    if (op==SYLE_GUEST_STATUS) return c->state;
    if (op==SYLE_GUEST_DESTROY) return dispose(c);
    if (c->state==SYLE_GUEST_QUARANTINED) return ERR(EQUARANTINED);
    if (op==SYLE_GUEST_CONSOLE_SPACE) {
        if (c->state!=SYLE_GUEST_RUNNING) return ERR(ESTATE);
        if (!c->backend.console_space) return ERR(ENOTSUP);
        int n=c->backend.console_space(c->context);
        return n>16 ? ERR(EBACKEND) : n;
    }
    if (op==SYLE_GUEST_CONSOLE_SEND)
        return c->backend.console_send ? c->backend.console_send(c->context,(uint8_t)arg) : ERR(ENOTSUP);
    if (op==SYLE_GUEST_CONSOLE_RECV)
        return c->backend.console_recv ? c->backend.console_recv(c->context) : ERR(ENOTSUP);
    if (op==SYLE_GUEST_START || op==SYLE_GUEST_STEP) {
        if (op==SYLE_GUEST_STEP ? c->state!=SYLE_GUEST_RUNNING :
            (c->state!=SYLE_GUEST_READY && c->state!=SYLE_GUEST_STOPPED))
            return ERR(ESTATE);
        if (c->backend.start(c->context)) {
            c->state=SYLE_GUEST_QUARANTINED;
            return ERR(EBACKEND);
        }
        c->state=SYLE_GUEST_RUNNING;
        return 0;
    }
    if (c->state==SYLE_GUEST_STOPPED) return 0;
    if (c->state!=SYLE_GUEST_RUNNING) return ERR(ESTATE);
    if (c->backend.stop(c->context)) {
        c->state=SYLE_GUEST_QUARANTINED;
        return ERR(EQUARANTINED);
    }
    c->state=SYLE_GUEST_STOPPED;
    return 0;
}
int64_t guest_control_call(struct guest_control *c, uint64_t caller,
                          uint64_t op, uint64_t handle, uint64_t arg) {
    if (!caller || caller!=c->manager) return ERR(EPERM);
    if (c->busy) return ERR(EBUSY);
    c->busy=true;
    int64_t result=call_locked(c,op,handle,arg);
    c->busy=false;
    return result;
}
int64_t guest_control_owner_exit(struct guest_control *c, uint64_t caller) {
    if (!caller || caller!=c->manager) return ERR(EPERM);
    if (c->busy) return ERR(EBUSY);
    c->busy=true;
    int64_t result=c->state==SYLE_GUEST_EMPTY ? 0 : dispose(c);
    c->busy=false;
    if (!result) c->manager=0;
    return result;
}
