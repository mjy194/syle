#ifndef SYLE_GUEST_CONTROL_H
#define SYLE_GUEST_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
#include "../include/syle/guest_control.h"
/* Single BSP, non-preemptible management entry. No SMP synchronization.
 * Trusted backend may be installed before publishing this controller only.
 * Every callback must restore a safe host context before returning, including
 * error returns. Unsafe host restoration must fail-stop, never return to user.
 * Quarantine tracks resource ownership, not permission to run with dirty host.
 * The four lifecycle callbacks return zero for success, nonzero for failure.
 * prepare owns ALL partial allocations until release succeeds, even on error.
 * start must return boundedly to management. Success means runnable, not Linux
 * boot success. stop must quiesce execution and restore host before returning.
 * release may free RAM only after VMCLEAR/INVEPT/VMXOFF as applicable succeeds.
 * Failed callbacks may retain ownership; controller never invents cleanup. */
struct guest_backend {
    int (*prepare)(void *context, uint64_t profile);
    int (*start)(void *context);
    int (*stop)(void *context);
    int (*release)(void *context);
    /* Optional bounded queues. send: 0 or negative ABI error; recv: byte or
     * negative ABI error (EBUSY for full/empty). Never host port passthrough. */
    int (*console_send)(void *context, uint8_t byte);
    int (*console_recv)(void *context);
};
struct guest_control {
    struct guest_backend backend;
    void *context;
    uint64_t manager, generation, handle;
    enum syle_guest_state state;
    bool busy, configured;
};
/* Fresh, unowned object only. Never reinitialize a live/quarantined object. */
void guest_control_init(struct guest_control *, uint64_t manager,
                        const struct guest_backend *, void *context);
int64_t guest_control_call(struct guest_control *, uint64_t caller,
                          uint64_t operation, uint64_t handle, uint64_t argument);
/* Called on management exit/fault. Does not release unsafe resources; failures
 * retain quarantine. The caller must not reuse its owner identity afterward. */
int64_t guest_control_owner_exit(struct guest_control *, uint64_t caller);
#endif
