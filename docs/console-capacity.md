# Serialized management RX capacity

`SYLE_GUEST_CONSOLE_SPACE` (25) takes the existing guest handle and argument zero.
The authorized manager may query it only in RUNNING state. PIC04 returns 0–16
available guest UART RX slots (non-FIFO capacity is one). Zero means full, not an
error. Unsupported profiles return ENOTSUP. Guest UART loopback returns zero external
RX slots, preserving pending host input during UART self-tests. Invalid device state, busy dispatch,
entry in flight, or an outstanding IRQ reservation returns a negative ABI error.
The query reads only: no RX read/write, overflow flag, timer or IRQ side effect.

This is **not a reservation**. The guarantee is limited to the current single-BSP,
nonpreemptible, serialized management ABI: query capacity, read at most one host
byte, then SEND it without an intervening STEP or another RX producer. A future
concurrent manager requires an atomic try-send operation or actual credits.

Existing SEND behavior is unchanged and may have device effects on failure.
Never retry a byte after SEND fails; stop and report the ambiguous transfer.
Capacity avoids full-FIFO retries, but it does not turn backend failures into
retryable errors. The host-input front end is separately opt-in and must not
expose arbitrary port access or guest pointers.

Tests cover owner/handle/argument/state gates, callback reentry, missing/invalid
backend responses, empty/full FIFO capacity, read-only equality, busy/inflight
and unsupported-profile rejection. The previous fixed-command shell proof is
independent of this new interface and remains reproducible.
