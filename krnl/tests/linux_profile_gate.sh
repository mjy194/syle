#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lpg-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
api=${SESSION_API_DIR:-krnl};apic=${GUEST_APIC_API_DIR:-krnl};decoder=${MMIO_DECODER_DIR:-krnl};devices=${DEVICE_API_DIR:-krnl}
sources="krnl/linux_profile_activation.c krnl/linux_profile_gate.c krnl/linux_cpu_profile.c krnl/linux_vcpu_policy.c krnl/linux_acpi_reduced_install.c krnl/linux_acpi_reduced.c krnl/linux_acpi_topology.c krnl/linux_apic_dispatch.c krnl/linux_ioapic_mmio.c krnl/linux_apic_mmio.c krnl/linux_apic_msr.c krnl/linux_mmio_memory.c krnl/linux_guest_irq_adapter.c $apic/guest_apic.c $apic/lapic.c $apic/lapic_timer.c $devices/ioapic.c $decoder/guest_mmio_mov.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$apic" -I"$decoder" -I"$devices" -I"$api" -Ikrnl -fsanitize=address,undefined -g krnl/host-tests/linux_profile_gate.c $sources -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$apic" -I"$decoder" -I"$devices" -I"$api" -Ikrnl -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_profile_gate.c -o "$tmp/model.o"
