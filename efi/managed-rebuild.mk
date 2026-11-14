# NEW kernel fixture only. Historical baseline hash gate remains unchanged.
MB_CHECK_SCRIPT = ../guest/linux-rebuild/check-inputs.py
MB_EMBED_TAG = rk
HC_TAG = rk
HC_OUTPUT_TAG = rebuild
HC_BITS = 20
HC_TSC_FREQ = 1
include managed-host-clock.mk
.PHONY: managed-rebuild
managed-rebuild: build/krnl-managed-rebuild-20-tsc.elf build/mgmt-guest-terminal.elf
