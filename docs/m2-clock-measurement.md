# Separate input latency, virtual time and execution cost

The first external-terminal observation measured a whole command phase,
including byte pacing: guest uptime advanced from 0.36 to 1.75 around sleep 1,
while the host command phase took 30.84 seconds. The harness used 150 ms per
input byte (roughly 7.8 seconds for the reported 52-byte command) plus receive
polling. That number is **not** the measured duration of the nanosleep syscall.

## Source facts, not yet a runtime attribution

`linux_guest_dispatch.c` normally increments a logical tick once per accepted
VM exit. `linux_pic04_idle.c` may jump a halted guest to an eligible PIT edge,
but only under its interrupt/ownership gates. RDTSC is intercepted and the
PIT2 model returns a synthetic timestamp derived from these ticks. The
PIC04 PIT and UART RX timeout are driven by this same logical clock. There is
no general host-wall-time synchronization contract. KVM entry/exit, guest
execution, manager calls and serial output all consume host time and can affect
the observation without an equal amount of guest-clock progress.

Thus exit-driven time is a plausible contributor, not proof that the whole
30.84 seconds is a clock bug. Input pacing, output queues, idle policy and
execution cost must be distinguished before changing the clock.

## Guest probe

The independent static program supports `clock0`, `clock10`, `clock100`,
`clock1000` (nanosleep requests in milliseconds) and `clockspin` (a fixed one
million-iteration scalar loop). Each run prints a short BEGIN marker, samples
CLOCK_MONOTONIC and CLOCK_MONOTONIC_RAW by direct syscall and RDTSC, performs the
operation, samples again, prints END and reports all three deltas. It uses no
vDSO/libc fast path. Sampling itself has cost, and the three samples are not
simultaneous. A backward result or failed syscall is a failure, not fabricated
success. `clock0` is a syscall/scheduler control, not a guarantee of zero cost.

Build a **new** initramfs and manifest with the updated program. Do not change
the historical fixture hashes or reuse an old archive containing the old ELF.
Use the existing `managed-baseline-terminal` target and isolated KVM run.

## Host observer

```
python3 tools/measure_guest_clock.py /absolute/run/console.sock \
  --output-dir /absolute/new-measurements --repeats 3 --pace-ms 150 --timeout 900
```

The tool waits for Linux ready and a shell prompt, then sends each command once.
It records send start/end, BEGIN/END receipt, guest deltas and final DONE. It
does not use matching characters in unrelated output as echo acknowledgments.
Results require exact marker lines and all metrics; malformed/duplicate results
fail closed. At the end it requests Ctrl-] cleanup and requires management exit
zero. Keep the outer VM timeout larger than the observer's deadline.

**Host timestamps are receipt times.** UART buffering, manager drain, guest
scheduling and host scheduling may delay either marker. In particular a BEGIN
and END received in one chunk do not prove zero guest elapsed time. The JSON
does not claim precise guest execution boundaries. `serial.log` is retained.

Compare repeated distributions and the slope of BEGIN-to-END host latency
against the measured guest deltas at 0/10/100/1000 ms; subtracting one noisy
clock0 sample is not a reliable correction. Compare spin to sleep to distinguish
active and idle advancement. Only afterward decide whether to change the
clock source, idle deadline scheduling, slice length or performance bottlenecks.
If ambiguity remains, add a bounded host TSC/exit-count diagnostic at the
hypervisor boundary instead of deriving an exact clock rate from UART alone.

Host tests validate the probe and parser; they are not KVM measurement results:

```
sh guest/linux-baseline/test-clock.sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/test_measure_guest_clock.py
# On an executor permitting local Unix sockets:
SYLE_CLOCK_SOCKET_TEST=1 PYTHONDONTWRITEBYTECODE=1 python3 tools/test_measure_guest_clock.py
```

The socket mock is opt-in because some cloud executors prohibit even AF_UNIX
sockets. A skipped socket mock is not a passing transport integration test.

## First isolated KVM observation and controlled slice experiment

The v3 run completed all five modes and checked cleanup. For 0/10/100/1000 ms
requests, host BEGIN-to-END receipt intervals were 0.09063/0.26866/2.29454/
22.27769 seconds; guest monotonic deltas were 3.84031/11.843608/103.850953/
1000.067877 ms. Input transmission separately cost 4.05–4.50 seconds. Spin was
8.52 ms of receipt interval but only 2.514 us on the guest synthetic clock.
These are single samples, not precision benchmarks. The sleep trend persists
outside input pacing; it does not establish exactly where host time was spent.

The independent `managed-clock.mk` target tests whether the VMX entry bound
contributes. Build both with the **same** v3 input archive and manifest:

```
make -C efi -f Makefile -f managed-clock.mk managed-clock CLOCK_BITS=28 \
 MB_INPUTS=/absolute/clock-inputs MB_MANIFEST=/absolute/clock-inputs/manifest.json
# Repeat with CLOCK_BITS=24; pair each krnl-managed-clock-{bits}.elf
# with the same mgmt-guest-terminal.elf in separate fresh ESP/VARS.
```

Run the same five-mode observer once per fresh VM, serially, then repeat in
reverse order if stable. Both builds add the same diagnostics; only the entry
bound differs (2^28 vs 2^24 host TSC ticks, not a calibrated duration). The
production default remains 28 and has no diagnostic output. Unsupported VMX
timer shifts fail prepare rather than causing a negative shift.

At destroy, `SYLE.CLOCK_DIAG` reports hexadecimal bits, exits, preempt exits and
entry_tsc_sum. Counts include only exits passing session stage-7 reason/vectoring validation; failed raw exits are excluded. Counts cover the entire session including boot/input/idle, **not**
individual commands. Entry TSC windows include `lvs_run` plus post-exit host
validation before dispatch; they are not pure guest CPU time. RDTSC is not
serialized, TSC frequency is not calibrated, and no per-exit I/O is added.
Keep raw logs; verify the diagnostic line appears before successful cleanup.
A speedup proportional to the smaller slice would support slice sensitivity,
not prove a complete wall-clock solution. No logical tick multiplier is added.
