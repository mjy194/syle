#!/bin/sh
set -eu
candidate=$1
prefix=$2
code=$3
vars=$4
make -C "$candidate/source/efi" \
 -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk \
 -f firmware-ovmf-clock.mk -f firmware-ovmf-apic.mk \
 -f firmware-ovmf-cache-kvm.mk -f firmware-ovmf-irq.mk \
 -f firmware-code-rom.mk -f firmware-pause.mk -f firmware-pm-delay.mk \
 -f firmware-halt.mk -f firmware-console.mk -f firmware-disk.mk \
 -f firmware-persistent.mk -f firmware-managed.mk \
 -f firmware-managed-runtime.mk -f firmware-ovmf-quiet.mk \
 firmware-disk "EFI_PREFIX=$prefix" "OVMF_CODE=$code" "OVMF_VARS=$vars"
