#!/usr/bin/env bash
# Host/build verification only: never starts QEMU, KVM, firmware or a guest.
set -euo pipefail
cd "$(dirname "$0")/.."
make all simple-guest
../krnl/tests/simple_guest_image.sh "$PWD/build/simple-guest.elf"
../krnl/tests/simple_guest_dispatch.sh
../boot/tests/policy.sh
../boot/tests/kernel_elf.sh "$PWD/build/krnl.elf"
../boot/tests/kernel_elf.sh "$PWD/build/krnl-simple-guest.elf"
../krnl/tests/boot_info.sh
tests/firmware_diagnostics.sh "$PWD/build/krnl.elf"
echo 'PASS simple guest prototype host/build checks; VM-entry and guest execution NOT tested'
