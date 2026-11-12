#ifndef SYLE_GUEST_CONTROL_ABI_H
#define SYLE_GUEST_CONTROL_ABI_H
#include <stdint.h>
/* int 0x80: rax=operation; rdi=handle (zero for create); rsi=argument.
 * No user pointers, host addresses, flags or arbitrary images are accepted.
 * Successful create returns a positive handle, status a state, console recv
 * a byte, and other operations zero.
 * REAP(0,0) cleans the owner slot after failed-create quarantine.
 * Errors are sign-extended negative enum values in rax. */
enum syle_guest_call { SYLE_GUEST_CREATE=16, SYLE_GUEST_START,
    SYLE_GUEST_STOP, SYLE_GUEST_STATUS, SYLE_GUEST_DESTROY, SYLE_GUEST_REAP, SYLE_GUEST_STEP,
    SYLE_GUEST_CONSOLE_SEND, SYLE_GUEST_CONSOLE_RECV };
enum syle_guest_state { SYLE_GUEST_EMPTY=0, SYLE_GUEST_READY,
    SYLE_GUEST_RUNNING, SYLE_GUEST_STOPPED, SYLE_GUEST_QUARANTINED };
enum syle_guest_error { SYLE_GUEST_EINVAL=1, SYLE_GUEST_EPERM,
    SYLE_GUEST_ENOENT, SYLE_GUEST_EBUSY, SYLE_GUEST_ESTATE,
    SYLE_GUEST_ENOTSUP, SYLE_GUEST_EBACKEND, SYLE_GUEST_EQUARANTINED,
    SYLE_GUEST_EEXHAUSTED };
#define SYLE_GUEST_BOOT_PROFILE 1u
#endif
