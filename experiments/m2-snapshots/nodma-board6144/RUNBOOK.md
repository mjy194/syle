# Isolated noDMA compact6144/board6144 diagnostic: NOT RUN

Exact base archive run26-source-snapshot.tar.gz SHA256
7d60c4fe7613f0b078296524bd3d7168c5088b0df9175a3902c2011d4cb07204.
Fresh extraction only; check overlay/BASE-SHA256SUMS BEFORE applying overlay,
then overlay/SHA256SUMS. Verify package CONTROL before any work. Never overwrite
default checkout, completed experiment or mainline frozen candidate.

## Delta and semantics

Keep full prior diagnostic PCI projection, oneCF8, synthetic PIIX identities,
noBAR sizing, livePM state, restrictedD000-DFFF block aperture. This is NOT a
single hostbridge BAR fix or equivalent to the mainline chipset profile.
All five noDMA modules byte-identical to executed4096. admit/read/write refuse
without target dereference; PFN cannot install; no actual disk backing or DMA.

Observer switches reviewed fot to foc lossless compact API, updates only trace
summary fields, and explicit exit cap4096->6144 authorized as a NEW experiment.
Do NOT import mainline PCI macro object. Set6144 delegated board-cycle bound:
the total exit limit still cannot exceed6144.2MiBbuffer,30s reference, per-entry VMX
slice, outer180s, clock/IRQ logic and owned-fetch/RIP writer unchanged. Encoder
CPU overhead can alter real timing; byte equality does not prove timing parity.

Compact copies text into bounded dictionary; refs and HEXLE64 tokens. Full
independent decoder validates retained stream before one-shot output. Two-pass
validation is not cryptographic integrity; private immutable state/buffer is
required. Full dict uses literal fallback. Overflow sticky failed/incomplete;
retained prefix may drain once, no later guest entry. Sink false may have partly
transmitted: never retry. Native UART callback is NOT delivery acknowledgement.

Previous4096 uncompressed run stopped at3705 on2MiB overflow. Retained prefix
showed block BAR64-byte sizing/restore, not allocation. Host resegmented replay
of2,097,136-byte retainedprefix encoded805,213B with exact reconstruction, but
resegmented events are NOT native call boundaries or a capacity/performance
proof. Incomplete/dropped result remains inconclusive; do not enlarge memory,
drop evidence, increase bounds, resume or retry. No guarantee of enumeration.

## Mandatory fresh gates

    sh PACKAGE/host-check.sh FRESH_OVERLAY_SOURCE
    sh PACKAGE/compact-host/test.sh

Seven host processes plus compact oracle/stress tests. Verify compact-host .c/.h
hashes equal overlay modules (they are exact reviewed copies). Host tests do not
prove native decoder/sink/clock execution. Build same quiet Makefile chain as
previous candidate with checked EFI_PREFIX/CODE/VARS. From fresh efi directory:

    make -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk -f firmware-ovmf-clock.mk -f firmware-ovmf-cache-kvm.mk -f firmware-ovmf-irq.mk -f firmware-ovmf-quiet.mk EFI_PREFIX=CHECKED_GNU_EFI_PREFIX OVMF_CODE=CHECKED_CODE_FD OVMF_VARS=CHECKED_VARS_FD firmware-ovmf-quiet

No production default switches.
Cloud ELF SHA256 e6ed26e8874d06b8cf6aa0d51c7497455f2dcf42dcf2faa3cceba99a83e74df3 is STATIC only.
Fresh GCC16 build: nm-u empty, no fot/vfr or firmware_ram_adapter, three refusal
bodies mov0ret, actualfoc andvfb calls; preserve loader8MiB span checks. Record
actual rebuiltELF/media hashes, do not assume cloud debugpath hashes match.

## Execution

WAIT for mainline compact actual result and root scheduling. Only inputs may
run, serialized after all VM cleanup, using unchanged strict privateQMP/PID/
starttime/CPU/device-map/paused-admission runner. Tiny/noHv controls, then at most
ONE positive; nohostNMI/no extra passthrough/no real disk. Preserve inherited
transientL1 WBINVD caution and cumulative ledger. Stop at unsupportedconfig/PFN,
any gate/unknowncompletion,6144exits,6144boardcycles,30s oroverflow. Inspect and
cleanup, never automatic restart. Capture all counters/log/QMP/source identity.

Report accepted logical/decoded/requested bytes, encoded bytes, events, dropped,
failed/incomplete and exact prefix boundaries. Only a complete trace supports
absence statements. Config discovery or sizing is not BAR assignment, driver
bind, queue/transfer or shell. Waiting for review/runtime does not authorize DMA.

## V2 exact-copy packaging correction

V1 was stopped by inputs before any VM: host header still had older overflow
marker wording while native had reviewed decoded-retained-prefix wording.
The host header now exactly copies native; native overlay/ELF unchanged. Do not
relax exact-copy verification. host-check.sh now mechanically compares both
.c/.h against packaged native and freshly overlaid source before compilation,
and checks compact-host/frozen.sha256. Fresh GCC16 all gates remain mandatory.
Prior cloud/review claims of complete header equality were incorrect; this fixes
that packaging evidence gap, not a native behavior bug. Old artifacts preserved.

## Board-cycle alignment experiment

Relative to executed compact v2, the ONLY native source change is the delegated
board-cycle limit 4096 -> 6144. Root authorized this new bounded observation;
it is not a device fix. Total-exit6144, noDMA, 2MiB,30s and overflow-stop stay.
Prior execution reached5697 exits/4096 board cycles with complete trace and
1494785 encoded bytes, leaving602367 bytes. At most447 extra exits fit the
unchanged total limit; capacity remains empirical, not guaranteed. Previous
BAR sizing occurred twice but no assignment/queue access was observed.
