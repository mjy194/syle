#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lgd-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
api=${SESSION_API_DIR:-krnl}
# SESSION_API_DIR points to krnl/ in the verified session API archive when the
# manager-owned headers are not yet integrated. Never substitute guessed types.
test -r "$api/linux_guest_session.h"
test -r "$api/linux_guest_manager.h"
devices=${DEVICE_API_DIR:-krnl}
apic=${GUEST_APIC_API_DIR:-krnl}
decoder=${MMIO_DECODER_DIR:-krnl}
sources='krnl/linux_nonpresent_pf.c krnl/linux_profile_activation.c krnl/linux_profile_gate.c krnl/linux_cpu_profile.c krnl/linux_acpi_reduced_install.c krnl/linux_acpi_reduced.c krnl/linux_acpi_topology.c krnl/linux_ioapic_mmio.c krnl/linux_apic_dispatch.c krnl/linux_apic_mmio.c krnl/linux_apic_msr.c krnl/linux_mmio_memory.c krnl/linux_guest_irq_glue.c krnl/linux_guest_irq_adapter.c krnl/linux_irq_glue.c krnl/linux_external_irq.c krnl/linux_irq_adapter.c krnl/linux_pit2.c krnl/linux_disabled_apic.c krnl/linux_invlpg.c krnl/linux_sysenter_msr.c krnl/linux_debug_disabled.c krnl/linux_pic_mask.c krnl/linux_pause.c krnl/linux_rtc.c krnl/linux_guest_dispatch.c krnl/linux_guest_snapshot.c krnl/linux_microcode_model.c krnl/linux_cpu_commit.c krnl/linux_vcpu_policy.c krnl/linux_vcpu_cr.c krnl/linux_gs_commit.c krnl/linux_vcpu_msr.c krnl/linux_efer_commit.c krnl/linux_msr_bank.c krnl/linux_vcpu_state.c krnl/linux_pf_guard_v5.c krnl/linux_vcpu_event.c krnl/linux_high_pf.c krnl/linux_high_pf_data.c krnl/linux_pt_diag.c krnl/linux_pt_permissions.c krnl/linux_uart_runtime.c krnl/linux_uart_model_io.c krnl/linux_uart.c'
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -I"$apic" -I"$decoder" -I"$devices" -Ikrnl -I"$api" krnl/host-tests/linux_guest_dispatch.c $sources "$apic/guest_apic.c" "$apic/lapic.c" "$apic/lapic_timer.c" "$decoder/guest_mmio_mov.c" "$devices/ioapic.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
for src in krnl/linux_guest_dispatch.c krnl/linux_guest_snapshot.c; do
 ${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -I"$apic" -I"$decoder" -I"$devices" -Ikrnl -I"$api" -c "$src" -o "$tmp/$(basename "$src").o"
done
