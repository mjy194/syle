# L1 Linux SMP bring-up

The experimental profile runs two guest VMCSs on one pinned Syle CPU. The AP has its own guest register state, xAPIC and timer. The two vCPUs share the guest EPT. OVMF discovers the CPU count via fw_cfg and installs the supplied MADT with local APIC IDs 0 and 1 and legacy PIC routing.

Build an x86-64 EFI-stub Linux kernel with the M2 guest disk drivers, ACPI, CONFIG_SMP=y and CONFIG_NR_CPUS=2. Use the existing M2 ext2 root as the base. The fixture adds a static test init. It reports the online CPU list and checks its executing CPU after binding to CPU 0 and then CPU 1.

    sh experiments/l1-smp/build-disk.sh "$bzimage" "$rootfs" "$efi_prefix" "$fixture"
    python3 m2/build.py "$build" "$efi_prefix" "$ovmf_code" "$ovmf_vars" --dual-vcpu

Reference boot using a snapshot of the fixture disk:

    qemu-system-x86_64 -machine q35,accel=kvm -cpu host -smp 2 -m 128M \
      -bios /usr/share/edk2/x64/OVMF.4m.fd -drive "if=virtio,format=raw,snapshot=on,file=$fixture/seed.img" \
      -display none -monitor none -nic none -serial stdio -no-reboot

Run the same image on Syle and enter the manager commands c, s and a:

    python3 m2/run.py "$build" "$run_directory" --seed "$fixture/seed.img" \
      --disk "$persistent_disk" --vars /usr/share/edk2/x64/OVMF_VARS.4m.fd

The success marker is SMP_DUAL_PASS. The test init stays idle after reporting its result.

## Firmware handoff and affinity verification

Linux 6.12.0 disables the BSP LAPIC through the spurious-interrupt vector
register, reprograms the local vectors and timer, then enables the LAPIC again.
The BSP keeps the masked firmware timer's phase across the handoff. Each vCPU
has its own local timer and accepts fixed IPIs. On this PC-compatible profile
the 8259 uses BSP INTR in the IMCR reset mode. In IMCR APIC mode the route
instead requires an enabled BSP LAPIC and an unmasked ExtINT LINT0.

The reference and Syle runs used the same kernel and seed image. This disk
mounts its ext2 root read-only and starts the affinity probe as PID 1.

| Artifact | SHA-256 |
| --- | --- |
| EFI-stub bzImage | `b4542ed81b236eadd8a1d46df5ef9dad7443ff00d4fe98b58dcde07d17c7f5d1` |
| Seed disk | `b4763adfd17b7d18a8cdbf21f223af9e47d245bff4c5f016f73c6cfdaaab307a` |

Both environments printed:

    SMP_ONLINE=0-1
    SMP_PIN requested=0 actual=0
    SMP_PIN requested=1 actual=1
    SMP_DUAL_PASS

Syle schedules the two L1 vCPUs on one execution CPU.

## Clocksource regression

The kernel uses a 250 Hz periodic tick. Before HPET was exposed, its watchdog
compared the 10 MHz TSC against refined-jiffies. The boot CPU initially
received its ticks from PIT IRQ0 and later from its LAPIC. A serial trace from the failing configuration showed 25 PIT
periods elapsed with only 10 IRQ0 receipts in about 100 ms. The watchdog
could consequently measure 976.5 ms of TSC time over 508.0 ms of jiffies.

The dual-vCPU profile now retains elapsed unmasked periodic ticks at the
device and presents them to the PIC or the target LAPIC as its interrupt
request latch becomes available. Both CPUs sample the same host reference
at the RDTSC exit and subtract the same guest origin.

Tick replay alone still produced a 156 ms discrepancy in one of five cold
boots. The profile now publishes an ACPI HPET table for a 32-bit, 10 MHz
main counter at 0xfed00000. Linux can read elapsed time directly for its
watchdog while timer interrupts await delivery. The main counter uses the
host time reference and supports stop, set, restart and 32-bit wrap. The
PIT and LAPIC continue to deliver interrupts. HPET interrupt routing and
legacy replacement mode are outside this profile.

To repeat the check, start the documented Syle launch with `--socket` and
a fresh run directory and disk copied from the seed for each cold boot.
From the repository root run:

    python3 experiments/l1-smp/check-clock.py "$run_directory" --observe 15

The checker records the serial transcript, waits for both affinity markers
and observes the guest for another 15 seconds. It checks Linux's clocksource
switch messages through manager pause and close. Acceptance requires the
kernel to register `hpet`, select and retain `tsc` and the watchdog to report no instability.

Five consecutive cold boots of the clean build passed with the kernel and
seed hashes above. In a separate run, a read-only inspection at manager
pause matched Linux's curr_clocksource pointer to clocksource_tsc and its
watchdog pointer to clocksource_hpet in the same kernel's symbol table.

## KVM on both L1 CPUs

Use a base ext2 root without the SMP probe installed, and a kernel with
built-in KVM and KVM_INTEL. These commands add a 64-bit L2 probe to the
SMP disk and build a Syle runtime with both features:

    sh experiments/l1-smp/build-disk.sh "$bzimage" "$rootfs" "$efi_prefix" "$fixture" --nested
    python3 m2/build.py "$build" "$efi_prefix" "$ovmf_code" "$ovmf_vars" --dual-vcpu --nested

Boot the resulting disk with the reference QEMU command above. For Syle,
use the documented launcher with `--socket` and a new disk copy, then run:

    python3 experiments/l1-smp/check-clock.py "$run_directory" --nested

The checker selects Ring3's nested boot command (`n`). An ordinary create
(`c`) continues to hide VMX. CPU 0 and CPU 1 each have their own VMXON
and VMCS12 state, hardware VMCS02 and composed EPT cache. A nested VM exit
returns to the VMCS01 and register bank of the L1 CPU that entered it.

The two tests run sequentially. PID 1 binds itself to a single L1 CPU and
checks its CPU number. Its child inherits that affinity, opens /dev/kvm
and creates a fresh VM with one 64-bit vCPU. Three KVM_RUN calls must
produce OUT 'A', OUT 'B' and HLT. The probe checks the final RIP (0x18)
and the 64-bit R8 and RAX values (0x1234567800000042) before reporting
success for that L1 CPU. This exercises two independent L2 instances.

The reference boot and two clean Syle cold boots passed with seed SHA-256
`22ab225f6ba1ecadb32411f78c2b6f4130802a019cd026317db70e599612219e`
and the bzImage hash above. Both Syle runs retained TSC through the 15-second
post-probe observation and returned to the original page count on release.

### Interleaved KVM workers

Build a separate fixture with `build-disk.sh ... --concurrent` and use the
same Syle runtime built with `--dual-vcpu --nested`. Run the fixture on
reference KVM using the QEMU command above, then run it on Syle with
`m2/run.py ... --socket` and the serial checker:

    python3 experiments/l1-smp/check-clock.py "$run_directory" --concurrent

The two workers bind to L1 CPU 0 and CPU 1. They both create their own
64-bit L2 and report ready before the coordinator releases either
worker. The coordinator releases both first entries before waiting for
their exits, then alternates subsequent resumes. Each worker checks eight
I/O exits and a final HLT. The L2s share the same guest physical layout, with
different R8 seeds and RAM contents at GPA `0x800`. On every exit the probe
checks RIP, RCX, R8, R9, R10, the updated RAM word and an adjacent sentinel.
The checker verifies all 18 exit records, checks TSC, and confirms
that VMX and page allocation return to baseline after closing the guest.

Reference KVM and three Syle cold boots passed the interleaved state
checks with seed SHA-256
`3b136997cda8956e0c0725f3aeeec34764c512ebc2f688750090b24b69b4258f`.
All Syle runs returned pages to baseline. Two kept TSC through the
15-second observation. In the second run, after both probes passed,
Linux reported excessive watchdog read-back delay on CPU 1 and switched
from TSC to HPET.

### Migrating one L2 vCPU

Build the disk with `build-disk.sh ... --migrate` and use the
`--dual-vcpu --nested` runtime. The reference boot command above runs the
same probe. For Syle, start `m2/run.py ... --socket` and run:

    python3 experiments/l1-smp/check-clock.py "$run_directory" --migrate

The probe creates one KVM VM, one vCPU and one RAM mapping. It pins the
same thread to L1 CPUs 0, 1 and 0 before the three KVM_RUN calls. The
probe checks the actual CPU before entry and after exit. It retains the
same vCPU file descriptor and checks RIP, RCX, R8, R9, R10, the updated
word at GPA 0x800 and its adjacent sentinel at every exit.

The two I/O exits stop at RIP 0x19. The second exit increments R8 and
continues the first run's memory accumulation. After the second migration,
HLT returns at RIP 0x1e with RCX zero and the same final R8 and RAM word.

The reference KVM run and both Syle cold boots passed the migration checks
with seed SHA-256
`a97d34f1d43c9c8f6cdb43ba49250358fdd60787cfed94d2e34f14f659e87761`.
Both Syle runs released the VMX and EPT resources and returned pages to
baseline. The second run fell back from TSC to HPET after the migration
probe completed, during the 15-second observation.

### Linux as a two-vCPU L2

L2 boots from initramfs and needs no virtual disk for this nested
virtualization check. The fixture disk belongs to L1 and carries the
runner, L2 kernel and initramfs.

Build the fixture with `build-l2-smp-disk.sh`. It needs a two-CPU
EFI-stub Linux image, the M2 base ext2 root, a GNU-EFI prefix and
`gen_init_cpio` from a Linux build. It packages a statically linked BusyBox
from `PATH` or the `BUSYBOX` environment variable. The optional last argument
supplies a different kernel for L2 while keeping the L1 kernel fixed.

    sh experiments/l1-smp/build-l2-smp-disk.sh "$bzimage" "$rootfs" "$efi_prefix" "$gen_init_cpio" "$fixture" "$l2_bzimage"
    "$fixture/l2-run" "$fixture/l2-kernel" "$fixture/l2-initrd" --smp-shell
    python3 experiments/l1-smp/check-clock.py "$run_directory" --l2-smp

The fixture uses one KVM VM with two vCPU file descriptors on separate L1
CPUs. Its ACPI topology describes two local APICs and the KVM I/O APIC.
PID 1 checks both CPU affinities, runs two pinned workers and opens an
interactive BusyBox shell on the L2 serial console. At `L2 busybox # `,
try `uname -s`, `cat /sys/devices/system/cpu/online` and `halt -f`. The
checker sends these commands plus a per-run token through the serial connection
and checks the replies. The runner waits for both vCPUs to enter KVM's HALTED
state with interrupts disabled. The checker also verifies Syle's resource
release and the L1 clocksource.

### Host parallel VMX and INVEPT

The `smp-nested` hardware fixture validates two separate host CPU owners
at a shared L2 barrier and checks local VMX teardown. The
`smp-nested-local-invept` fixture additionally verifies real per-owner
INVEPT and the remote owner's cache behavior. Run both with KVM and two
QEMU vCPU threads bound to distinct physical cores. The success markers
are `SMP.N32B_PASS` and `SMP.N34B_PASS`.

The Ring3-managed profile also supports two Syle host CPUs. Build and launch
with `--host-parallel`. The launcher uses QMP to pin the two QEMU vCPU
threads to different physical cores before boot and records their affinities.

    python3 m2/build.py "$build" "$efi_prefix" "$ovmf_code" "$ovmf_vars" --dual-vcpu --nested --host-parallel
    python3 m2/run.py "$build" "$run_directory" --seed "$fixture/seed.img" --disk "$disk" --vars "$host_vars" --socket --host-parallel
    python3 experiments/l1-smp/check-clock.py "$run_directory" --l2-smp --host-parallel

Run the launcher and the checker in separate terminals. Use a fixture built
with the two pinned L2 userspace workers. The profile gives each L1 vCPU a
private VMX owner and VMCS on its assigned host CPU. VMCS02 runs on that
same owner. The BSP waits for both VM exits before emulating devices or
changing EPT. Shared EPT invalidation waits for the AP hardware IPI and its
local INVEPT. Composed EPT invalidations run on the owning CPU.

The checker verifies overlapping native VM-entry intervals, both L2
exit states after Linux halt, the same guest after pause and resume, and
release of both owners and guest pages. The AP sleeps outside VMX operation
after destroy. `affinity.json` and `serial.log` contain the run evidence.

The 100 Hz L2 fixture passed native KVM and two Syle cold boots with
seed SHA-256 `322bf5ba4c292116f06ed22bba9e7dbc01072796433af9eaabb6f7544634f2dc`.
The original 250 Hz kernel also reached both affinity markers and
both vCPUs halted. A VMX preemption on a dual-vCPU L1 retains the L2 VMCS
and register bank until its next run slot. Pending L1 LAPIC interrupts
are reflected with their acknowledged vector when VMCS12 enables
external-interrupt exiting and acknowledgement on exit.

Short groups of HPET and TSC reads with guest IF clear keep either L1
CPU's watchdog sample from including a peer run slot. The final
interleaved-L2 regression kept the L1 TSC through a 60-second
observation. The L2 kernels run their own clocksource watchdog.

The 250 Hz fixture also passed the full serial checker, including page
and VMX release and the 15-second L1 clock observation. Both kernels
completed the native KVM check using the same inputs as their Syle run.
On the final Syle runtime, a separate load run kept TSC selected while
both pinned L1 tasks performed 60 seconds of computation.

With the original 250 Hz kernel, the interactive BusyBox fixture passed
both native KVM and the Syle host-parallel run. The Syle serial transcript
shows command replies, both L2 vCPUs halted, and VMX and page release after
resuming and closing the same L1 guest.

### L2 clocksource

Add `--trace-l2-clock` to the dual-vCPU nested Syle build, regenerate the
SMP disk fixture, and run the regular serial checker with a clock interval:

    python3 experiments/l1-smp/check-clock.py "$run_directory" --l2-smp --host-parallel --l2-clock 30

The checker samples the L2 clocksource three times, with two nominal
30-second idle windows and no serial output between samples. At each sample
`l2-clock` records the local LAPIC and IRQ0 counts from `/proc/interrupts`
and alternates tagged RDTSC reads on L2 CPUs 0 and 1. The optional Syle
trace pairs tagged reads with the host's 100 ns reference counter and the
physical TSC at VM exit. Interrupt rates use the actual host elapsed time.
The trace is emitted after shutdown.

The original 250 Hz fixture both matched the host counter and had a
0-tick inter-vCPU bias across 24 samples, but delivered only about 89–90
and 134 local timer interrupts per host second. There were no IRQ0 events
in these windows. The same kernel delivered about 251 local timer interrupts
per host second on both CPUs under native KVM. The L2 watchdog compared TSC with `refined-jiffies`,
which falls behind when the periodic timer cannot deliver each tick.
Advertising the KVM TSC deadline timer alone still reproduced the issue.

The SMP runner now supplies a clocksource-only HPET. Its counter uses
`CLOCK_MONOTONIC_RAW` in L1 and advances independently of L2 timer
interrupts. L2 Linux can still use its normal watchdog, with HPET as the
reference and TSC as the selected source. LAPIC interrupt delivery is
unchanged, and LOC counts can still be below the configured tick rate.
In a cold host-parallel run of the 250 Hz fixture, both Linux guests
retained TSC through roughly 75 host seconds between the first and last
L2 sample. The L2 scheduler clock reported stable, and the usual shutdown
and resource release checks passed.

### L2 LAPIC tick rate

For the accelerated clock-event profile, add both `--nested-vmx-timer`
and `--trace-l2-clock` to the host-parallel build, and build the SMP seed
with `L2_CLOCKEVENT=tsc-deadline`. This retains the original guest kernel.
Then run:

    python3 experiments/l1-smp/check-clock.py "$run_directory" --l2-smp --host-parallel --l2-clock 15 --l2-tick-hz 250

The tolerance is 5% for each vCPU in each window. The trace counts L0
delivery of L1 LAPIC timers, virtual VMX timer expirations reflected to
L1, and L2 LAPIC vectors submitted and delivered via VMCS02. The guest
counts its own handler invocations independently.

In the slow path, L0 delivered nearly every L1 timer and L2 received
nearly every vector submitted by L1. Temporary KVM diagnostics in L1
showed that after L2 switched to one-shot clock events, L2 rarely had a
second expiry pending to merge. Expiry to reprogramming took about 5 ms
on one CPU and 11 ms on the other in the instrumented run. The next
interval programmed by L2 averaged 2.5–3.1 ms. Adjusting idle polling
alone did not solve the shortfall.

The accelerated path lets KVM use a virtual VMX preemption timer backed
by the host clock. L2 programs absolute TSC deadlines rather than
relative LAPIC counts. The runtime batches VMCS access on the remote
owner and uses a validated shared host reference page when available.
It retains a short VMX slice near expiry and longer slices away from it.
The L2 HPET remains the clocksource watchdog reference.

With the original 250 Hz kernel in both guests, the final acceptance run
measured 247.1 and 247.9 interrupts/s on CPU 0, and 246.5 and 247.6 on CPU 1
over two roughly 20-second host windows. Both clock checks retained TSC;
the BusyBox command round trip, VM-entry overlap, and managed teardown
checks also passed.
