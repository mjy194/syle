# Restricted legacy fw_cfg observation profile

The observed OUT word510h with selector0 matches QEMU's x86 fw_cfg control register. This is an ISA I/O interface, not a newly invented PCI function. Selector510h accepts LE16; data511h is a byte stream. Selection resets the offset; reads past a known item's end return zero and advance. The immutable profile supplies signature QEMU, ID=1 (traditional interface only), actual prepared128MiB RAM, one CPU/current+maximum count and a big-endian empty file directory. Initialization takes prepared EPT RAM size and the actual single-vCPU configuration and rejects another profile.

ID does not advertise DMA. No514h DMA descriptor or guest address is followed, no ramfb, e820, ACPI or SMBIOS file is claimed. File lookup through the empty directory reports no files; unknown selectors stop rather than imitate QEMU's general zero-stream behavior. This is an explicitly incomplete fail-stop profile. Later memory detection may fall back to CMOS34/35; those bytes are not invented here.

Pinned OVMF2025.02 selects BaseIoLibIntrinsicSev. With the exposed non-SEV CPU it uses CLD; REP INSB for FIFO reads. Therefore this candidate implements more than selector acceptance: scalar known-item reads and a bounded actual-memory REP transaction.

## REP admission and ownership

Only exact F3 6C, reason30 byte/string/REP/IN port511, DX511, 64-bit flat code, DF0, TF0, RF0 and no debug/entry/vectoring/blocking are admitted. VMX_BASIC bit54 must report INS/OUTS instruction-information support; only defined address-size bits9:7 are checked for64-bit, while undefined fields are ignored. ES must be usable/present/writable CPL0 data with base0, and exit GLA must equal current RDI. Native RCX/RDI are the remaining-count and current-destination state, not guessed original values.

Count0 completes without accessing memory or advancing the stream. Count1..64 must fit one page. Every byte is pretranslated through owned guest page tables with writable permissions and already-set A/D requirements; no A/D updates or PF handling are added. The target cannot be ROM/VARS/MMIO, either source instruction page, or any page-table GPA/HPA on this transaction's two-byte instruction and destination walks. This is not a scan of every page table in the guest. Legitimate broader accesses can stop because this is a restricted observer, not a full x86 fault emulator.

The entire supported remaining operation is validated before the first byte is written. Input bytes and cursor are staged privately. RAM is then written, followed by RIP; RCX=0, RDI+=count and cursor publish only after success. RAX remains unchanged for REP INSB. If RAM writing fails after some bytes or VMCS commit fails after all bytes, memory may already have changed: the session becomes permanently failed and never resumes. No rollback or partial-fault architectural continuation is claimed. Tests inject both failures and verify no GPR/cursor publication and no retry.

## RF distinction

Intel SDM vol3C28.3.3 says instruction-caused exits controlled by VM-execution controls save RF=0, even if RF was1 before execution; INS/OUTS are covered by26.1.3. This I/O profile therefore requires RF0 and performs no RF write. It does not confuse that rule with reason52 preemption (preserve RF) or EPT instruction completion (separately modeled). Unexpected RF1 on an I/O exit stops for diagnosis rather than being blindly cleared.

## Validation scope

Host tests cover all six wire items, cursor/tail/overflow, every unknown selector, immutable-state isolation, target ownership/A-D/boundaries and both walk-path aliases, actual scalar/REP opcodes and state updates, missing VMX information, ES/GLA/count/debug constraints, zero count and partial-write/RIP failure. DMA/string forms outside this bounded REP path stop. Native KVM execution and the resulting next limitation remain to be measured; no full fw_cfg or display success is claimed.
