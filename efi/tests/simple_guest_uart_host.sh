#!/usr/bin/env bash
# Focused host/build checks for the COM1 increment. Never starts a VM.
set -euo pipefail
cd "$(dirname "$0")/.."
make all simple-guest simple-guest-uart
../krnl/tests/guest_uart.sh
../krnl/tests/simple_guest_io.sh
../krnl/tests/simple_guest_image.sh "$PWD/build/uart-guest.elf"
../krnl/tests/simple_guest_dispatch.sh
echo 'PASS guest UART host/build checks; VM-entry and guest execution NOT tested'
