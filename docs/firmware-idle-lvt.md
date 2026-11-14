# Idle LVT-only observation increment

Native diagnostics measured ESI=0x20005 at MOV [RBX],ESI to FEE00320: periodic, unmasked vector 5, with old LVT=0x10000, initial count=0 and armed=false. This candidate permits that register setup only while all timer execution state remains zero. It does not start a timer or deliver an interrupt.

The guard admits aligned 32-bit LVT writes with only vector, mask and one-shot/periodic bits. Deadline mode, reserved/status bits and other widths fail. Every nonzero initial-count write still fails before mutation, regardless of mask or vector. ICR, self-IPI and other unmasked LVTs remain prohibited. Timer current-count is zero because no timer has started, not because elapsed time is ignored.

This supersedes earlier guard documentation requiring timer LVT masked at all times. Source-free/event-free invariants still hold. The next expected stop is a nonzero initial-count write; only real execution can establish it. No IF=0 or large-count argument is used to discard an event. Vector-5 illegal-delivery/error semantics and a calibrated timer/IRQ lifecycle remain separate future work.

Host guard and exact ROM instruction tests pass, including both possible RFLAGS/RIP VMWRITE failures retaining old device/GPR state and fatal-stop status. Active observer builds and has no undefined symbols. Tiny ROM and active observer must be rerun serially on KVM after independent review.
