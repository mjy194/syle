# Isolated guest NMI experiment (NOT YET EXECUTED)

Only the designated VM test runner may execute this after the firmware/KVM
queue is idle. Never target a physical host, host kernel, BMC, sysrq or OS
security setting. This procedure targets a freshly launched single-vCPU QEMU
**guest** through that process's private Unix QMP socket. No parallel firmware
VMs. Stop on any uncertain command result; do not blindly retry inject-nmi.

## Artifacts and immutable inputs

- Build test managers with `nmi_probe_build.sh NEW_DIR`. Mode 0 creates a READY
  session; mode 1 creates and STARTs once to reach RUNNING. Each then prints
  its specific WAIT marker and spins in CPL3 at exported `nmi_probe_wait`.
  CREATE/START failure prints FAIL and never enters the admitted wait loop.
  These ELF programs contain guest int80 calls: DO NOT execute on the host.
- Build an isolated kernel using ROLE_BINDING_LINK=1 NMI_GUARD_LINK=1
  NMI_PROBE_LINK=1 managed_final_link.sh NEW_DIR. The only extra instrumentation
  is `nmi_probe_owner_exit_calls`, incremented at entry to guest_owner_exit in
  the isolated ring3 copy. This counter does not change the default tree.
- Retain kernel/manager SHA256, kernel map/nm, exact source snapshot, baseline
  bzImage/initramfs hashes, OVMF CODE/VARS hashes, and disk/ESP hashes. Record
  QEMU version, full command line and private QMP socket/PID. Use a fresh VARS
  copy and fresh disposable overlay per mode; never edit baseline media.
- The kernel remains the managed host-clock diagnostic profile. The runner
  must provide its known external hv-time/VMX contract and existing validated
  boot wrapper. Do not guess a replacement CPU/firmware command line. Verify
  boot wrapper filenames load THESE kernel/manager hashes from the overlay.

## Admission before injection

1. Queue owner confirms no other firmware/KVM run active. Launch one fresh
   QEMU process with `-S` (initially paused), exactly one vCPU, captured serial output, private Unix
   QMP socket and the disposable media. Store PID, /proc/PID/stat starttime,
   executable and exact command. Require the QMP socket path in that process's
   arguments and verify socket ownership/inode against that same live PID;
   reject PID/socket reuse. Use the established isolated device graph only:
   read-only OVMF CODE, fresh disposable VARS and ESP/overlay, serial capture,
   private QMP, no physical disks/devices, no host network/port forwarding or
   passthrough. Save the complete graph/command, not just selected flags.
2. Connect to that socket, consume QMP greeting, execute `qmp_capabilities`.
   Verify `query-status` reports running=false and status prelaunch or paused
   (fresh -S may be prelaunch); `query-cpus-fast` must identify exactly one
   guest CPU. Retain its cpu-index, qom-path and thread-id; ensure it belongs to
   the admitted PID/starttime. Select this exact CPU in HMP (cpu-index 0 only).
   Only after identity/device checks pass, send `cont`.
   Do not connect to a preexisting/unidentified QMP endpoint.
3. Wait for the exact `NMI.PROBE.READY_WAIT` or `NMI.PROBE.RUNNING_WAIT` serial
   marker. Any FAIL/missing marker/time limit cancels injection.
4. Execute QMP `stop`, wait for its STOP event and verify query-status
   running=false before register/memory reads. Use `human-monitor-command` with `info registers` to
   read the paused guest CPU. Require CS privilege 3 and RIP at the wait loop
   identified from THIS manager ELF's nm/disassembly. For the frozen managers,
   mode0 permits only RIP 0x8000000052 or 0x8000000054; mode1 permits only
   0x8000000082 or 0x8000000084 (pause/jmp instruction boundaries). A marker alone is not
   proof the print syscall has returned. If paused in kernel, do not inject;
   explicitly resume and recheck or abort under the runner's control.
5. Resolve `nmi_probe_owner_exit_calls` from THIS kernel ELF. Read its guest
   physical memory using HMP `xp /1gx ADDRESS`. Require zero before injection.
   Kernel is identity mapped at its fixed link base; verify load addresses
   from retained map, do not substitute a host address. Mandatory BEFORE
   snapshots: complete managed_guest and actual lgs session objects, PMM stats,
   available_count and available extents. Use nmi_probe_inventory.py's exact
   symbol SIZE+name resolution (there are TWO symbols named session; never
   pick the first). Decode expected READY/RUNNING state and nonzero handle,
   prepared/enabled/host.active plus nonzero control/RAM/EPT ownership. Use
   QMP pmemsave/HMP guest memory read into new artifact files only.

## Exactly one guest-only injection

While stopped at the verified CPL3 wait loop, send QMP:

    {"execute":"inject-nmi"}

Require a successful response, then send `cont`. This queues an NMI in the
single QEMU guest, not the host. If the response is lost/uncertain, mark the run
inconclusive and stop; do not send a second NMI to 'make sure'. Nested-NMI/IST
behavior is outside this minimal test.

## Evidence and outcome

Capture serial through the existing vector-2 fatal report and PANIC.COMPLETE,
then stop the guest again, wait for STOP and verify running=false before
reading the owner-exit counter and snapshots. Success requires:
- admitted READY/RUNNING state before injection and paused CPL3 wait-loop RIP;
- vector 2 fatal path observed, no return to manager/guest execution;
- owner-exit counter still zero afterward;
- mandatory AFTER snapshots of the same controller/session/PMM spans are
  byte-identical to BEFORE, including expected state/handle and ownership
  inventory. Any unexpected difference is inconclusive/failure, not retention;
  without these snapshots the only conclusion is hook-not-called plus fatal;
- no unexpected exception, restart, timeout or backend state mismatch.

The expected result is fatal-stop with resources retained, NOT successful
teardown or NMI recovery. The counter/readback and serial log are both needed:
absence of a cleanup message alone is insufficient. Record QMP `quit` response and verify this exact PID/starttime exits; clean up
only its private socket/disposable files after preserving evidence. Stop/close this isolated
VM only after artifacts are captured; do not reuse its potentially dirty state.
Run the other manager mode in a NEW VM/overlay. Preserve failures/inconclusive
runs, never relabel them PASS.

This tests one actual virtual CPU NMI path. It does not prove physical-host
NMI handling, nested NMI/IST robustness, console NMI safety, native mapping
leases or DMA safety. The scoped native-map candidate remains fail-stop.
