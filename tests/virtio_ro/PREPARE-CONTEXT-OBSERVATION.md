# Actual pre-prepare context: one isolated observation

2026-11-13: inputs ran the reviewed isolated fail-closed kernel once. Independent
verification re-read physical captures, rebuilt the current CR3 reachable tree,
rehashed exact source byte ranges, decoded controller/session bytes and compared
the actual paging root data with captured CR3. This is not a Linux boot test.

Evidence archive Library ID:
`libfile_fde3_f-e4af647404f08191aa267c6722dc00a5_ab3ac89c6f9c81918c711418a26b62e9`

Archive SHA256 `e4093fd620a768148cb66a027c12a829b531e6bd40f2c808726a431029970b41`,
17,787,111 bytes. Kernel SHA256
`80ef29010473361af1bffc9feb2f31384ba8d1cf5d5f46d85116e333ea0d1c1c`.
Replay: `python3 tests/virtio_ro/verify_prepare_capture.py EXTRACTED_CANDIDATE`.
Recorded independent result: `evidence/prepare-context-independent-replay.json`.

- Captured RIP 0x4003a9b, CS 8, SS 0x10, CPL0, SMM0, IF/DF clear.
- CR3 0x100000 equals the uniquely identified paging.c root value. CR0
  0x80010033, CR4 0x668, EFER 0xd00. This differs from earlier CPL3 READY CR3.
- Controller state1/handle1/busy1 is an in-progress CREATE, not a ready VM.
  Session configured1, prepared/enabled/host.active/memory.prepared/state.ready0;
  EPTP/VMXON/VMCS0; all 32768 RAM and67 EPT ledger entries zero.
- Replayed262 table pages and832 captured physical pages (837 pmemsave calls
  including5 object snapshots). Kernel984064B, initrd1332736B, command74B hashes
  match frozen inputs. All source-overlapping reachable leaves are supervisor
  RO/NX; no writable overlap found in this current CR3 tree.
- One cont, STOP, read-only capture, then quit acknowledged/exit0/reaped. No
  inject-nmi or second cont. Runner elapsed9.3177s. Hardware observation does
  not rely on running the CREATE to completion: the isolated helper is followed
  by unconditional leave_context and failure, never lgs_prepare or VMX entry.

This closes the earlier observation-point gap: actual enter_context occurred
before the snapshot. service.reset already ran; the diagnostic changes timing.
The source audit covers outer-QEMU guest virtual→physical, not nested Linux EPT,
other CR3s, host physical, device DMA, future mapping changes or global aliases.
No source mapping lease or persistent immutability is proved. Native mapping
refusal remains enabled. Virtual-disk integration must independently establish
owned target RAM, stable backing and failed-store/commit semantics.
