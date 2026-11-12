#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-pic04-combination.XXXXXX)
trap 'rm -rf "$out"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Ikrnl krnl/host-tests/linux_pic04_integration.c krnl/linux_nonpresent_pf.c krnl/linux_profile_activation.c krnl/linux_profile_gate.c krnl/linux_cpu_profile.c krnl/linux_acpi_reduced_install.c krnl/linux_acpi_reduced.c krnl/linux_acpi_topology.c krnl/linux_ioapic_mmio.c krnl/linux_apic_dispatch.c krnl/linux_apic_mmio.c krnl/linux_apic_msr.c krnl/linux_mmio_memory.c krnl/linux_guest_irq_glue.c krnl/linux_guest_irq_adapter.c krnl/linux_irq_glue.c krnl/linux_external_irq.c krnl/linux_irq_adapter.c krnl/linux_pit2.c krnl/linux_disabled_apic.c krnl/linux_invlpg.c krnl/linux_sysenter_msr.c krnl/linux_debug_disabled.c krnl/linux_pic_mask.c krnl/linux_pause.c krnl/linux_rtc.c krnl/pit0.c krnl/pic0.c krnl/pic0_ports.c krnl/pic0_glue.c krnl/pit0_transaction.c krnl/guest_wait.c krnl/legacy_irq_runtime.c krnl/linux_port80.c krnl/linux_feature_control.c krnl/linux_misc_features.c krnl/linux_msr_gp.c krnl/linux_idle_pit.c krnl/linux_i8042_absent.c krnl/linux_int3.c krnl/linux_guest_dispatch.c krnl/linux_guest_snapshot.c krnl/linux_microcode_model.c krnl/linux_cpu_commit.c krnl/linux_vcpu_policy.c krnl/linux_vcpu_cr.c krnl/linux_gs_commit.c krnl/linux_vcpu_msr.c krnl/linux_efer_commit.c krnl/linux_msr_bank.c krnl/linux_vcpu_state.c krnl/linux_pf_guard_v5.c krnl/linux_vcpu_event.c krnl/linux_high_pf.c krnl/linux_high_pf_data.c krnl/linux_pt_diag.c krnl/linux_pt_permissions.c krnl/linux_uart_runtime.c krnl/linux_uart_model_io.c krnl/linux_uart.c krnl/guest_apic.c krnl/lapic_timer.c krnl/guest_mmio_mov.c krnl/lapic.c krnl/ioapic.c krnl/pic_master_04.c krnl/pic_master_04_glue.c krnl/linux_pic04_domain.c krnl/linux_pic04_runtime.c krnl/linux_pic04_idle.c krnl/linux_progress_diag.c -o "$out/test"
ASAN_OPTIONS=detect_leaks=0 "$out/test"
for f in krnl/linux_pic04_domain.c krnl/linux_pic04_runtime.c krnl/linux_pic04_idle.c krnl/linux_guest_dispatch.c krnl/linux_progress_diag.c; do
 cc -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -Ikrnl -c "$f" -o "$out/$(basename "$f").o"
done
# Native entry configuration with the actual EFI include path available here.
efi=${EFI_INC:-/usr/include/efi}
for defs in '' '-DMG_LEGACY_PIC' '-DMG_EXPERIMENTAL_APIC' '-DMG_LEGACY_PIC04' '-DMG_LEGACY_PIC04 -DMG_PROGRESS_DIAG'; do
 cc -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -DMG_FIXED_INPUTS_VERIFIED $defs -I"$efi" -I"$efi/x86_64" -Ikrnl -Iinclude -c krnl/linux_guest_startup.c -o "$out/startup.o"
done
if cc -std=c11 -ffreestanding -DMG_FIXED_INPUTS_VERIFIED -DMG_LEGACY_PIC -DMG_LEGACY_PIC04 -Ikrnl -Iinclude -c krnl/linux_guest_startup.c -o "$out/conflict.o" 2>"$out/conflict.log"; then
 echo "conflicting profiles incorrectly compiled" >&2; exit 1
fi
grep -q 'Legacy PIC and experimental APIC profiles are mutually exclusive' "$out/conflict.log"
