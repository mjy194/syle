# Same-guest high-resolution clock pause test

This is separate from terminal Ctrl-] cleanup. It creates one guest and starts
one persistent Linux process. The process samples monotonic/raw/TSC, records
its PID, prints ARMED, then blocks reading stdin. The manager STOPs the same
handle, verifies STOPPED, prints the handle and PAUSED, and polls only the host
UART until the observer sends `R` after two host-monotonic seconds. No guest
STEP, destroy, or create occurs in this interval. It then STARTs the same
handle and sends a newline into guest stdin. The original process checks its
PID, samples again, reports deltas and success. The manager checks success and
cleans up. This is not an ACPI guest suspend or host-system sleep test.

The kernel's diagnostic START tick is emitted only on a stopped-to-running
transition. The observer requires the first STOP tick to equal the second START
tick exactly. Guest deltas across the whole protocol include real guest activity
before STOP and after START, including output/input processing; they need not
be zero and must not be called the paused-duration error. The measured external
PAUSED-receipt to `R`-send interval is a lower bound on the actual stopped time.

Build a NEW probe and NEW initramfs/manifest using the preserved BusyBox and
same fixed kernel. Never modify the prior clock-v3 archive or hashes:

```
sh guest/linux-baseline/build.sh /new/clockpause-probe
python3 guest/linux-baseline/make-initramfs.py --busybox /verified/busybox \
 --probe /new/clockpause-probe --output /new/inputs/initramfs.cpio \
 > /new/inputs/manifest.json
# Copy the fixed bzImage to /new/inputs/bzImage, retaining its hash.
make -C efi -f Makefile -f managed-clock-pause.mk managed-clock-pause \
 HC_BITS=20 HC_TSC_FREQ=1 MB_INPUTS=/new/inputs MB_MANIFEST=/new/inputs/manifest.json
```

Pair `krnl-managed-host-clock-20-tsc.elf` with the **new**
`mgmt-guest-clock-pause.elf`, not the external terminal manager. Use the same
verified QEMU `-cpu host,hv-time=on` and isolated ESP/VARS, no network, one vCPU.
The manager automatically enters the guest command; the observer must not type
shell commands. Start it on the runner's host console socket:

```
python3 tools/measure_guest_pause.py /run/console.sock \
 --output-dir /run/new-pause-measurements --pause-seconds 2 --timeout 900
```

Keep an outer VM timeout greater than 900 seconds and complete source/input/
binary/command hashes. Retain raw, report and cleanup. Never retry sending `R`
if its send result is uncertain. An admission, read, PID, sample, identity or
cleanup failure is not success. A host mock is not native acceptance.

The existing resume probe's normal entry point remains a wrapper with its old
commands and no pause callback. Its regression suite is retained. New tests
cover callback invocation only while STOPPED, same handle, failed callback
without START, exact frozen ticks and PID/metric rejection. The clock and
CPU profile remain experimental and make no hard-real-time guarantee.

Host tests before running:
```
sh user/mgmt/tests/guest_resume_probe.sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/test_measure_guest_pause.py
# On archbook where AF_UNIX is permitted, require the transport mock too:
SYLE_CLOCK_SOCKET_TEST=1 PYTHONDONTWRITEBYTECODE=1 python3 tools/test_measure_guest_pause.py
```
The transport mock waits for the single `R` byte and checks it is delayed at
least the requested interval. Cloud default skips this socket test explicitly.

## Verified isolated KVM result

Evidence Library `libfile_fde3_f-daee29c31b8c8191bd095e223a10e95a_ab3ac89c6f9c81918c711418a26b62e9`
contains two fresh runs with checked manager exit zero and no residual VM.
Both retained handle 1 and Linux PID 20. First STOP/second START were exactly
3,199,954 ticks in run 1 and 3,214,713 in run 2. Host PAUSED receipt to resume
send measured 2,004,537,664ns and 2,007,220,941ns. Guest monotonic deltas spanning
the whole protocol were 8,640,761ns and 8,938,285ns; these include activity on
both sides and are not a pause error estimate. The package also records the
new fixture hashes and all source/build/command evidence. This verifies this
explicit management-freeze policy, not host suspend, live migration, multiple
vCPUs or hard-real-time delivery.
