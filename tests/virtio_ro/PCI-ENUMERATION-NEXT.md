# Minimum PCI enumeration work after single-latch integration

Current real blockers, not hypothetical missing tests:

* `fpi_io` exposes only first16 bytes of00:00.0. Any header read past0x0f or
  present-header write stops. Generic enumeration/BAR/resource code cannot
  safely treat this as a complete configuration function.
*00:01.0 currently returns absent. Normal multifunction scanning may therefore
  never reach00:01.3. Its vendor/device header read also stops: `fpm_config`
  implements only exact PMBA dword40 and PMREGMISC byte80, not identification.
*00:02.0 is discoverable in the new host wrapper only. Its BAR is limited to a
  reserved4KiB IO window; OVMF resource allocation outside it will still stop.

## Smallest proposed change boundary (not implemented or native enabled)

Add one independent optional config-image layer to the wrapper, keeping existing
fpp/fpi defaults intact. Keep exactly the same single address latch. Decode each
present function to a complete256-byte config image, permitting aligned1/2/4-byte
reads within it. Unknown/absent functions remain all-ones. Unimplemented BARs read
zero even during probe; do not invent MMIO windows, PCI bridges or capabilities.

Keep current00:00.0 vendor/device/class. Define a coherent00:01.0 ISA bridge
function0 with multifunction flag, and00:01.3 PM function identity; select/pin
these platform identities against the actual OVMF chipset driver before using
an experimental candidate. Do not silently copy physical host/q35 PCI IDs into
this synthetic i440fx platform. Identity choice can trigger new chipset probes;
that is an explicit compatibility risk, not proof that ISA/SMI/SCI is emulated.

PMBA and PM enable must remain projections of the one live fpm_state, not duplicate
register copies. Preserve fixed B000 decode, zero SCI/SMI and all unsupported
power-management behavior. Plan byte/word merges for legal PCI writes, with
reserved masks and side-effect handling separated from read-only identity bytes.
Command/status needs explicit RO/RW/W1C semantics; a blanket writable256-byte
array would falsely advertise behavior. BAR resource assignment must fit the
reserved IO aperture and actual firmware allocation plan. Unknown side-effectful
chipset writes should fail terminally until supported, not be acknowledged blind.

## Host gates before another guest candidate

1. Enumerate bus0 using ordinary function0/multifunction scan, reaching exactly
   hostbridge, ISA fn0, PM fn3, block fn0. Read all256 bytes at each legal width;
   test absent buses/functions and enable-bit-clear cycles.
2. Probe every BAR and restore; reserve overlaps and wraparound; verify command
   decode changes and PMBA/enable consistency across mixed-width transactions.
3. Preserve existing fpp timer behavior and single CF8 latch; test clock sampling
   that persists before failed RIP commit. No claim of whole-state rollback.
4. Combine actual firmware RAM copier, bus and block engine in the same process,
   then test RIP failure after completion and actual partial mapping failure.
   `firmware_bus_disk.sh` now covers this data/commit subset, not full enumeration.
5. Only then freeze a separate candidate for one serialized guest probe; unknown
   config probes remain diagnostic blockers. IRQ/multihead/storage kernel config
   and native mapping lifetime/fence remain separate prerequisites for Linux.
