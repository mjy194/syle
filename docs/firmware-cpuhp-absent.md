# CPU hotplug interface deliberately absent

Observed OUT32 AF00=0 at RIP82CE09 (EF). Matching EDK22025.02
PlatformInitLib/Platform.c:611-663 and QemuCpuHotplug.h identify the
PIIX4 CPU hotplug probe: two OUT32 AF00=0, OUT8 AF05=0, IN32 AF00.
The source expressly treats a completely absent block as ignored writes and
all-ones read, then uses MaxCpuCount=BootCpuCount (fw_cfg supplies1).

QEMU protocol reference:
https://qemu-project.gitlab.io/qemu/specs/acpi_cpu_hotplug.html
It describes the modern block atAF00, command-data2 dword offset0 and command
byte offset5. Reserved registers of a PRESENT modern device readzero; this
profile does not emulate such a device and must NOT returnzero to the probe.

The model admits only the above operation forms, with zero assessed at actual
operand width (unused RAX bits ignored). IN32 produces RAX=00000000FFFFFFFF;
OUT retains RAX. All other accesses stop rather than applying broad absent-I/O
behavior. No selector, present bitmap, hotplug command, SCI/SMI, S3 or CPU
addition/removal is implemented. There is exactly one fixed guest CPU.

Integration reuses PIIX scalar decoder and owned instruction fetch, CPL0,
exclusive ownership, reference sampling, clone, RIP commit, then RAX publish.
Failed commit is terminal and leaves RAX unchanged; the sampled time may have
advanced, not rolled back. Native outer dispatch does not advance RIP again.
Neither the default Linux target nor another I/O port gains new behavior.

Pure exhaustive tests cover all ports, widths/directions and representative
operands. Transaction regression includes exact source sequence, zeroextension,
nonzero command rejection, RIP failure and permanent rejection of retry.
This is source-backed absence discovery, not hotplug support. Native result
for this candidate remains pending.
