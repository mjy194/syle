#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mode=${1:-run}
QEMU=${QEMU:-qemu-system-x86_64}
OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}
OVMF_VARS=${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}
[[ -r "$OVMF_CODE" && -r "$OVMF_VARS" ]] || { echo 'Set OVMF_CODE and OVMF_VARS to matching non-secure OVMF firmware files.' >&2; exit 2; }
command -v "$QEMU" >/dev/null || { echo "QEMU executable missing: $QEMU" >&2; exit 2; }
serial=stdio
if [[ ${NO_UART:-0} == 1 ]]; then serial=none; fi
esp=build/esp
extra=()
video=(-device "VGA,romfile=${VGA_ROM:-vgabios-stdvga.bin}")
if [[ ${NO_GOP:-0} == 1 ]]; then video=(-vga none); fi
if [[ -n ${QMP_PORT:-} ]]; then extra+=(-qmp "tcp:127.0.0.1:$QMP_PORT,server=on,wait=off"); fi
if [[ -n ${QEMU_DATA:-} ]]; then extra+=(-L "$QEMU_DATA"); fi
case "$mode" in
  run) ;;
  ud|pf) esp=build/$mode-esp;;
  debug) extra+=(-S -gdb "tcp:127.0.0.1:${QEMU_GDB_PORT:-1234}");;
  ring3-[0-4]) esp=build/$mode-esp; extra+=(-device isa-debug-exit,iobase=0xf4,iosize=0x04);;
  smp-nested-atomic-multiround|smp-nested-atomic-largepage|smp-nested-atomic-gigapage|smp-nested-atomic-uc) esp=build/$mode-esp; extra+=(-device isa-debug-exit,iobase=0xf4,iosize=0x04);;
  smoke) esp=build/smoke-esp; extra+=(-device isa-debug-exit,iobase=0xf4,iosize=0x04);;
  *) echo 'Usage: scripts/run.sh [run|debug|smoke|ud|pf]' >&2; exit 2;;
esac
esp=${ESP_DIR:-$esp}
[[ -r "$esp/EFI/BOOT/BOOTX64.EFI" ]] || { echo 'Build EFI binary first.' >&2; exit 2; }
# Fresh variables every run keep smoke results independent of NVRAM history.
runtime_vars=${OVMF_VARS_RUNTIME:-build/vars-$mode.fd}
cp "$OVMF_VARS" "$runtime_vars"
# Stage 0 deliberately uses TCG. This does NOT validate VMX or nested Hyper-V.
exec "$QEMU" -machine q35,accel=tcg -cpu "${QEMU_CPU:-max}" -smp "${QEMU_CPUS:-1}" -m "${QEMU_MEMORY:-256M}" \
  -drive "if=pflash,format=raw,unit=0,readonly=on,file=$OVMF_CODE" \
  -drive "if=pflash,format=raw,unit=1,file=$runtime_vars" \
  -drive "format=raw,file=fat:rw:$esp" \
  -net none "${video[@]}" -display "${DISPLAY_BACKEND:-none}" -serial "$serial" -monitor none -no-reboot \
  "${extra[@]}"
