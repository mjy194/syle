# CODE ROM diagnostic only

Based on actual PM1 evidence: Library evidence SHA256 57e27a5c27e48eb62b34b3131f175f9c9df619942201f2a9535a1919b067665b, archbook syle-pm1-20261113.fSUney. All three original files are byte-identical to that archived source snapshot. PM1 completed 5167 dispatches; terminal RIP70af26b/GPAffc84010/q5aa; cumulative WBINVD36 as last reported by runner. Reconcile any newer runs before future work.

Static finding only: fixed CODE PE offset436f1c, VirtHstiDxe, RVA26b C60050; caller RVA957 returns95c, matching actual stack70af95c with inferred base70af000. CODE baseffc84000; offset10 byte78. Flash.c probes50/read/70/read to classify ROM. Runtime actual bytes, GLA and ownership are still missing. This is not a #PF or permission to make CODE writable.

Candidate adds fmm_diagnose_code_write plus one stop-only branch for GPAffc84010. Logs owned instruction bytes, saved GPRs, GLA, guest page-table walk, actual CODE EPT permissions/matches and original byte. No raw host tokens are logged. The branch never acknowledges or ignores the store: it returns false and stops before instruction completion. It can verify the FIRST probe only; later70 probe remains unexecuted/unverified. Existing validators are unchanged. No CODE write/flash implementation.

The existing exit path's clock_sync remains before the diagnostic, preserving its previous sampling behavior. The added diagnostic performs no clock read, guest write, VMCS write, register/device-state publication, A/D repair, EPT edit or resume. Do not describe the entire exit path as device-state-free: the inherited clock synchronization can update virtual state.

Host test uses strict C11 -Wall -Wextra -Werror -O1 with ASan/UBSan and SYLE_FMM_DE_CONFIG. Tests exact owned RX/0x35, guest read/write translation, absent D/RW, alias and leaf mismatch, unchanged full physical backing, and existing validators rejecting CODE. Independent reviewer reran PASS. Native fresh build from actual PM1 archived source succeeded. Original loader inspect/load PASS: file4987976 span7634944 zero_fill3271676 guards_intact. nm-u empty. ELF SHA c2badb1a0e69ae8677f4c3bdb9de7ee4f0afeb3f6c9fdeea4bc46ddc670a1e7e.

No VM was started. Parent reported VM runner currently blocked by safety review. This artifact does NOT authorize, retry or bypass that operation. Do not run a VM while that restriction remains. Future authorized target build still requires source-base verification, fresh rebuild and original loader check; all existing limits/controls/identity/cleanup requirements remain. Ledger unchanged by this work.

Apply only the three .c/.h files into krnl of a NEW copy of actual PM1 source. The overlay is diagnostic-only, not a runnable image. Native makefiles remain unchanged; DE fmm and PM1 SCI consumers retain their existing matching build macros.
