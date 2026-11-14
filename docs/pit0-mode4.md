# PIT channel 0 mode 4

The observed Linux control byte `0x38` selects channel 0, low/high byte access,
binary mode 4 (software-triggered strobe). It is a real oneshot timer mode, not
a shutdown no-op. Existing mode 2 behavior and rejected unsupported controls
are preserved.

The model follows the Intel 8254 datasheet, “Mode 4: Software Triggered Strobe”
and the counter-reading note (Intel document 231164, mirrored at
https://www.scs.stanford.edu/23wi-cs212/pintos/specs/8254.pdf): load on the next
clock, decrement starting on the following clock, then one-clock low OUT at
terminal count. The subsequent rising edge increments the IRQ edge counter
once. The counter continues wrapping at 16 bits afterward, but OUT does not
periodically strobe. Gate is fixed high in this channel-0 model.

Relative to the completed low/high write at tick T and count N:

- count loads at T+1;
- OUT is low at T+N+1;
- OUT rises at T+N+2, producing one IRQ0 edge.

Counts 1 through 65536 are valid (encoded zero means 65536). Mode 2 still
requires at least 2. First-byte reload writes do not affect counting; the second
byte schedules a next-clock reload. A low OUT during that write stays low until
the next clock; mode reprogramming itself raises a low OUT immediately.
Counter latching, time overflow and edge-count overflow preserve atomic failure.

Both legacy and PIC04 idle planners use `pit0_next_edge()` rather than assuming
all active counters are periodic. A completed mode-4 strobe has no future timer
edge until reprogrammed, so idle time advances normally instead of repeatedly
jumping to a past deadline or fabricating periodic interrupts.

Tests cover dense versus sparse polling, counts 1/65536, 16-bit wrap without
retrigger, partial/two-byte reload, low-OUT reprogramming, latched reads,
transaction failure, one IRQ0 and post-strobe idle behavior:

```
sh krnl/tests/pit0.sh
sh krnl/tests/linux_idle_dispatch.sh
sh krnl/tests/linux_pic04_integration.sh
sh krnl/tests/pit0_transaction.sh
```

These are model/dispatcher tests, not proof of completed Linux shell interaction.
