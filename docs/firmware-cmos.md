# Firmware cold-boot CMOS adapter

Native next stop was OUT byte 70h with AL=0Fh. Matching OVMF SecMain.c `IsS3Resume()` selects CMOS 0Fh and compares the data byte to FEh. This platform only supports cold reset, not S3 resume. A private shutdown-status byte returns 00h and accepts only idempotent zero clears; it is not read from host CMOS.

The adapter reuses `linux_rtc` scalar-I/O validation, index selection, NMI-mask latch, calendar and RIP/state commit. Shared-model extensions are guarded by `LRTC_FIRMWARE_COLD_BOOT`: index 0Fh read-zero/write-zero and read-only memory-size bytes 34h/35h and 5Bh–5Dh. Without that explicit macro, including the normal Linux build, 0Fh remains unsupported. Firmware blocks every other data-port write, including the generic model's normally permitted idempotent A/B writes. Port70 reads, non-byte/string/REP forms and unknown data registers remain unsupported. Index selection may store an arbitrary 7-bit index; unknown data access then fails without inventing a value.

Index bit7 is a private NMI mask latch only. No NMI source or delivery is connected; it does not manipulate host NMI state, provide pending-NMI behavior or claim NMI functionality. The existing observer handles any unsupported NMI/event dependency by stopping.

For supported RTC reads, time is explicitly synthetic UTC 2000-01-01 plus elapsed time from the SAME admitted Hv 10MHz source used by the virtual APIC. It is not the host's current time. Conversion is `seconds*1193182 + floor(reference_remainder*1193182/10000000)`, preserving subsecond rate without treating APIC ticks as PIT ticks. Reversal, changed epoch and calendar beyond 2099 stop. The outer clock adapter independently checks CPU/source admission and maximum sampling gap.

A failed RIP write retains old index/NMI/calendar state and AL and sets permanent failure; no retry/resume is permitted. Successful port70 selection commits its private latch only after RIP. The firmware wrapper persists last raw reference only after successful RTC transaction; any failed transaction is terminal, so time cannot be retried backward.

Host tests cover 0Fh cold status, bit7 select/clear, unknown register, data writes, read70, width, uncertain RIP write, default-build 0Fh rejection, shared-Hv conversion at one-second/fractional boundary, synthetic date and epoch/reversal failures. Native KVM confirmed the shutdown-byte clear. The newly added memory-size fallback still awaits native validation.

## Memory-size fallback

Actual next native access was index5Dh IN71h. Matching PlatformInitLib/MemDetect.c reads 5Dh..5Bh for RAM above4GiB, and34h/35h for RAM above16MiB in64KiB units. The pure `fcmi` admission model accepts only canonical RAM base0/128MiB with consistent flash/reset regions. Native initializes CMOS only after successful owned memory preparation; validation failure follows cleanup. Bytes34h=00h,35h=07h encode128MiB total, and5Bh..5Dh=00h encode no upper RAM. Writes to these bytes are forbidden.

These bytes are size descriptors, not an e820 file or final usable HOB map. Low legacy reserved regions and firmware allocations must remain reserved by OVMF; physical backing is not synonymous with allocatable memory. The128MiB-to-flash hole and addresses above4GiB are not advertised as RAM. Final guest HOB contents have not yet been observed. No E820, hotplug or S3 support is added.
