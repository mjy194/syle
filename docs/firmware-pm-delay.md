# PM timer wait and bounded runtime budget

The subsequent [HLT idle/wakeup probe](firmware-halt.md) resolved the HLT
rejection described here and verified three repeated interrupt/EOI cycles.

The 2026-11-14 native experiment completed the UEFI Shell startup countdown:
five real one-second PM timer waits, followed by a different stop at HLT
(`reason=12`, `RIP=0x70d09e0`). The final run completed 86,431 dispatches under
a 131,072 cap. It did not reach the exit, reference-time or trace-capacity
limits. Interactive UEFI Shell and firmware-to-Linux boot remain unaccepted.

## What the firmware was waiting for

The pinned OVMF CODE image has SHA256
`624e06de18b4fa535e90db7160d00d3d07d206422b89999bf1e27d920264e4e0`.
The authoritative hash is also checked by `tools/analyze_firmware_pm_delay.py`.

Its decompressed Metronome PE is at offset `0x1566bc`, SHA256
`e84dbfdb21c113a1bf4420ff12179f1a1d312b2de3a833e828b92472c388aba9`.
The observed load base is inferred as `0x70c2000`. The relevant instructions
read B008 at `0x70c231b`, compute `EAX = EBX - returned_timer`, test bit 23,
and execute `PAUSE` at `0x70c236e` while that bit remains clear. Equality still
waits. Only the low 24 bits determine this wraparound comparison; raw EAX can
exceed `0xffffff` after a timer wrap and is not itself an elapsed duration.

Runtime snapshots showed `EDI=0x369e99` (3,579,545 ticks) and `R12=0`:
each request is exactly one second with no extra 2^22-tick chunks. The PM timer
continues to derive its 3,579,545 Hz count from the existing 10 MHz reference.
No synthetic increments, sleeps, or skipped guest reads were introduced.

The caller chain identifies DxeCore's wait wrapper and `CoreStall`, then
Shell return address `0x685b97e`. The pinned Shell PE at decompressed offset
`0x486f14` has SHA256
`31b63d7ea99f12bfb833ee9ab4a8e32bf9cb0280b454cdd326280f54d1671cca`.
At inferred base `0x683d000`, its call site sets ECX to 1,000,000 microseconds,
calls the Stall boot service, and decrements the countdown. Its default is 5.
This corresponds to the startup-script delay in
[EDK2 Shell.c](https://github.com/tianocore/edk2/blob/master/ShellPkg/Application/Shell/Shell.c)
and the conversion in
[EDK2 Stall.c](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Misc/Stall.c).
The fixed binary and runtime records, rather than the current upstream branch,
establish the values used by this experiment.

The final read-only probe checked actual call-site bytes and owned RAM. It
observed configured delay 5 and saved caller RSI values **5, 4, 3, 2, 1**.
Frame lookup uses return addresses: the PAUSE boundary has one fewer frame
than the stopped IN boundary. The earlier fixed-index probe missed these
values; its run is preserved separately and is not the final probe acceptance.

## Measurements and budget decision

| Run | Exit cap | Measured progress | Stop |
| --- | ---: | --- | --- |
| Prior PAUSE implementation | 6,144 | First second: 68.910 ms, 931.090 ms remaining | Exit cap |
| Sampled-log baseline | 6,144 | First second: 59.447 ms, 940.553 ms remaining | Exit cap |
| First expansion | 32,768 | First second complete; second: 653.315 ms | Exit cap |
| Eight-frame diagnostic | 6,144 | Identified Shell caller | Exit cap |
| Countdown trial | 131,072 | All five seconds complete; countdown-value probe missed frame | HLT |
| Final probe correction | 131,072 | Five countdown values and five completed waits | HLT |

The sampled baseline estimated about 15,062 additional exits to finish its
first second, justifying the 32,768 trial. That trial exposed the repeated
calls. Binary/caller inspection then established the five-second countdown,
leading to the 131,072 profile. Each native stage was a separate, single
attempt with fresh media and tiny/no-Hyper-V controls.

Only repeated known-loop log blocks are sampled every 256 exits. The explicit
snapshot/target-transition records, rejection evidence and IRQ receipts remain.
This is **not** a complete record of every exit. All instruction validation,
PM reads, clock samples, IRQ decisions and VMCS commits still execute.
The 30-second reference limit, per-entry VMX slice, 2 MiB compact trace and
180-second outer timeout remain in force.

## Final verification

- Requested durations: five times 3,579,545 ticks. Actual crossing overshoots:
  45, 77, 135, 61 and 315 ticks. Sum of the five measured waits: **5.000177 s**;
  this excludes time between calls.
- PM reads: 40,151; completed PAUSE instructions: 40,141. Completed dispatches:
  86,431. Terminal reference elapsed: 18.7804003 s, below 30 s.
- Compact trace: 1,804,817 / 2,097,152 bytes; failed/incomplete/dropped all zero.
  Selected logical trace: 4,524,935 bytes. The subsequent HLT stop is recorded
  after the fifth crossing, proving execution left the delay loop.
- Tiny control exited 33; no-Hyper-V control exited 35 at clock admission.
  Positive run exited 35 in 158.203 s with cleanup, no timeout, unchanged QMP
  device graph/module admission, and no remaining QMP socket.
- Original kernel ELF parser/load ASan+UBSan checks passed: file below 5 MiB,
  loaded span 7,680,000 bytes below 8 MiB; no undefined ELF symbols. The
  eight-frame owned-RAM stack suite and five offline analysis tests passed.
- Five positive native attempts in this work each recorded one architectural
  L1 WBINVD attempt, taking the existing ledger from 39 to 44. This is not a
  count of physical cache flushes. Host settings were unchanged.

Final kernel SHA256:
`2dedb96de6761630e9b63dc4744723ea690e4f2eb0e5ef301ad4f0a11de690b3`.
Final raw serial SHA256:
`d8726fedabe1fbfa1dac787f240d02e76a34683fac7d515809a172fd3b010879`.

Evidence is under `efi/build/firmware-pm-delay-20261114-final/`, especially
`summary.json`, `expanded/analysis.json`, source/media manifests, loader/build
logs and `expanded/positive-once/serial.log`. Earlier stages remain in the
`firmware-pm-delay-20261114`, `-callers` and `-countdown` sibling directories.
The generator and build/analysis commands are documented in
`experiments/m2-snapshots/pm-delay-progress/README.md`.

## Next milestone boundary

The next observed missing behavior is firmware HLT idle/wake handling. The
fixed CpuDxe binary contains `HLT; RET` at the inferred stopped address; stack
records have moved into Shell's event/input path. Supporting this requires
the halted-vCPU and pending-interrupt/wakeup contract, not a PAUSE-style
unconditional RIP advance. Storage/display/input and a firmware-launched
lightweight Linux shell remain separate M2 acceptance work.
