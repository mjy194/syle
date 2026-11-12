#!/bin/sh
# Host-only contracts. No VM, guest, QEMU or firmware execution.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
CC=${CC:-cc}
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
run() {
 name=$1; shift
 "$CC" -std=c11 -Ikrnl -Wall -Wextra -Werror -fsanitize=address,undefined -g "$@" -o "$tmp/$name"
 "$tmp/$name"
}
for n in linux_boot linux_boot_reservations linux_boot_tss linux_guest_memory linux_vcpu_host linux_vcpu_host_alias linux_vcpu_prepare; do sh "krnl/tests/$n.sh"; done
for n in linux_high_pf_data linux_high_pf linux_efer_commit linux_pt_diag; do run "$n" "krnl/tests/$n.c" "krnl/$n.c"; done
run uart krnl/tests/linux_uart_io.c krnl/linux_uart_io.c krnl/guest_uart.c
run cpu krnl/tests/linux_cpu_commit.c krnl/linux_cpu_commit.c krnl/linux_vcpu_policy.c
run gs krnl/tests/linux_gs_commit.c krnl/linux_gs_commit.c krnl/linux_vcpu_msr.c
for n in linux_pf_guard linux_pf_guard_v5; do run "$n" "krnl/tests/$n.c" "krnl/$n.c" krnl/linux_vcpu_event.c; done
run permissions krnl/tests/linux_pt_permissions.c krnl/linux_pt_permissions.c krnl/linux_pt_diag.c
for n in linux_vcpu_policy linux_vcpu_event linux_vcpu_msr; do run "$n" "host-tests/$n.c" "krnl/$n.c"; done
run cr host-tests/linux_vcpu_cr.c krnl/linux_vcpu_cr.c krnl/linux_vcpu_policy.c
run state krnl/host-tests/linux_vcpu_state.c krnl/linux_vcpu_state.c
run backend -DLVB_HOST_TEST krnl/host-tests/linux_vcpu_backend.c krnl/linux_vcpu_backend.c
run msr-bank krnl/host-tests/linux_msr_bank.c krnl/linux_msr_bank.c krnl/linux_vcpu_state.c
run uart-model krnl/tests/linux_uart.c krnl/linux_uart.c
run uart-model-io krnl/tests/linux_uart_model_io.c krnl/linux_uart_model_io.c krnl/linux_uart.c
run uart-runtime krnl/tests/linux_uart_runtime.c krnl/linux_uart_runtime.c krnl/linux_uart_model_io.c krnl/linux_uart.c
run external-irq krnl/tests/linux_external_irq.c krnl/linux_external_irq.c
run irq-adapter krnl/tests/linux_irq_adapter.c krnl/linux_irq_adapter.c krnl/lapic.c krnl/ioapic.c krnl/linux_external_irq.c
run acpi-topology krnl/tests/linux_acpi_topology.c krnl/linux_acpi_topology.c
run acpi-install krnl/tests/linux_acpi_install.c krnl/linux_acpi_install.c krnl/linux_acpi_topology.c
sh krnl/tests/guest_apic.sh
sh krnl/tests/linux_apic_msr.sh
run guest-irq-adapter krnl/tests/linux_guest_irq_adapter.c krnl/linux_guest_irq_adapter.c krnl/guest_apic.c krnl/lapic.c krnl/lapic_timer.c krnl/ioapic.c
sh krnl/tests/guest_mmio_mov.sh
sh krnl/tests/linux_mmio_memory.sh
SOURCE="$root" APIC_DEPS="$root/krnl" IOAPIC_DEPS="$root/krnl" OUTPUT="$tmp/apic-irq-combo" sh krnl/tests/run_linux_apic_irq_combo.sh
sh krnl/tests/linux_apic_mmio.sh
sh krnl/tests/linux_apic_dispatch.sh
sh krnl/tests/linux_guest_irq_glue.sh
sh krnl/tests/linux_cpu_profile.sh
run acpi-reduced krnl/tests/linux_acpi_reduced.c krnl/linux_acpi_reduced.c krnl/linux_acpi_topology.c
run acpi-reduced-install krnl/tests/linux_acpi_reduced_install.c krnl/linux_acpi_reduced_install.c krnl/linux_acpi_reduced.c krnl/linux_acpi_topology.c
sh krnl/tests/linux_profile_gate.sh
# Native harness uses CPL3 refusal/fixup and user FX state only. Do not apply
# sanitizer instrumentation across the intentional FX-state exchange.
"$CC" -std=c11 -Ikrnl -Wall -Wextra -Werror -mgeneral-regs-only -mno-red-zone -fno-pie -no-pie krnl/host-tests/linux_vcpu_native.c krnl/host-tests/linux_vcpu_native_fx.S krnl/linux_vcpu_native.c krnl/linux_vcpu_native.S krnl/linux_vcpu_state.c -o "$tmp/native"
"$tmp/native"
python3 krnl/tests/linux_cpu_v8_budget.py
printf '%s\n' 'PASS host contracts only; Linux boot and VM execution not tested'
