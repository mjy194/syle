# Host-only firmware timer staging prototype

This is not connected to the verified active observer. No KVM result or IRQ/timer integration is claimed. The next native candidate remains read-only decoded-LVT diagnostics. The observed failing instruction writes ESI to LVT timer; the proposed `0x20005` value is inferred from matching source, not yet measured.

`firmware_timer_stage` reuses the existing `lapic_timer` counter model. A nonzero opaque clock-domain identity is checked on every call. Its unit is explicitly synthetic: 1024 caller ticks per undivided timer tick. Neither this identity nor host tests establish a calibrated clock, elapsed guest time, or a VMX wakeup schedule. A future integration needs all three before admitting guest execution with a running counter.

## State and ownership

- Idle: initial count zero or one-shot completed. LVT fixed-delivery one-shot/periodic configuration may contain low vectors. Merely storing a vector does not deliver an interrupt.
- Masked running: current count and periodic phase advance honestly. Expirations are consumed as masked, without later replay upon unmask. An invalid requested MMIO operation rolls back even these staged expirations.
- Unmasked running before deadline: permitted only for profile vectors 32–255. Countdown preserves its phase across LVT changes. Divider changes while armed remain unsupported.
- Unmasked due: STOP_IRQ, unchanged state. The timer still owns the unconsumed expiration. Reads, masking, cancellation and reprogramming cannot erase it. No IRQ sink, EOI, APIC priority arbitration or injection is invented. Caller must persistently latch the stop outside staged device state, prohibit all further model calls (including init), and must not resume guest execution. Because rejected calls preserve state, the pure model itself cannot detect a later fabricated pre-deadline timestamp or prevent reinitialization. Initializing a new object is permitted only for a fresh, terminated-and-recreated guest session. All samples must be globally monotonic including rejected operations. This lifecycle gate is a required future integration component, not implemented here.

Vectors 0–15 need illegal-vector/error-status semantics if an interrupt is generated; vectors 16–31 are not labelled architecturally illegal here. The prototype conservatively refuses *all* unmasked running vectors below 32. Therefore inferred OVMF idle `0x20005` can be represented, but its subsequent nonzero initial-count write is STOP_VECTOR. Supporting that sequence still requires an explicitly reviewed policy, not an assumption that IF=0 or a large initial count prevents expiry.

Every API operation stages a copy; error, clock-domain mismatch, backward time, overflow and unowned IRQ leave device state and output unchanged. Caller hardware transactions must likewise stage and publish only after success. There is no transfer/acknowledge API: actual IRQ ownership remains future work.

## Tests

`sh krnl/tests/firmware_timer_stage.sh` runs ASan/UBSan and a freestanding compile. Cases cover all eight divider encodings, current count, one-shot disarm, periodic multi-expiry and phase preservation, mask-before-deadline, unmask-after-masked-expiry, exact-deadline mask/cancel rejection, low-vector setup/arming boundary, backward/different clock domain, arithmetic overflow and invalid-MMIO rollback. No existing observer, generic timer model or Linux path is changed.

## Independent session gate

`firmware_timer_session` supplies the previously missing persistent latch as a separately tested host prototype. It initializes once in fresh zeroed owner-private storage; STOP_IRQ and STOP_VECTOR are irreversible through its API. Domain mismatch and backward observations also terminate the session. Every admitted time observation is retained even if the requested MMIO rejects. Hardware transaction failure calls `ftss_abort`, which cannot erase an earlier IRQ stop reason. Device state still preserves the unconsumed event.

This outer object must never be copied back from a speculative snapshot or reinitialized by the caller. It contains no IRQ sink or delivery acknowledgement: ownership stays with the timer until the entire guest session is terminated. The only available gate outcome for a due unmasked IRQ is stop. This is not an entry-scheduling API; integration still must poll immediately before guest entry and arrange a bounded wakeup at the deadline using a verified clock. Register transactions must not publish device mutations before hardware commit. The current prototype is intentionally not wired to native VMCS transactions.
