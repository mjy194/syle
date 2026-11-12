#!/bin/sh
set -eu
# Host only. Caller provides reviewed dependency directories; never starts VM.
: "${SOURCE:?source tree required}"
: "${APIC_DEPS:?reviewed guest_apic/lapic/timer directory required}"
: "${IOAPIC_DEPS:?reviewed ioapic directory required}"
: "${OUTPUT:?host executable destination required}"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -I"$APIC_DEPS" -I"$IOAPIC_DEPS" \
 "$SOURCE/krnl/linux_apic_msr.c" "$SOURCE/krnl/linux_guest_irq_adapter.c" \
 "$SOURCE/krnl/tests/linux_apic_irq_combo.c" \
 "$APIC_DEPS/guest_apic.c" "$APIC_DEPS/lapic.c" "$APIC_DEPS/lapic_timer.c" \
 "$IOAPIC_DEPS/ioapic.c" -o "$OUTPUT"
"$OUTPUT"
