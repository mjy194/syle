# Experimental profile and bounded page-fault support

This increment adds reviewed profile activation/owned-RAM preparation helpers and
same-domain host lifecycle tests. The default managed guest does not install the
experimental profile. Native prepare-hook wiring is available only through the separate
`managed-apic-profile` target. The default managed target remains unchanged in
profile selection. Both targets include the reviewed callback-context repair and
bounded CREATE/slice/PF diagnostics; runtime acceptance remains separate.

Nonpresent page-fault recovery is bounded to the documented CPL0 error-code 0/2,
owned memory, handler/frame, repeat and budget checks. It does not relax EPT or
exception interception. Host contract tests and a clean managed-kernel link are
not Linux boot acceptance or hardware IRQ verification.
