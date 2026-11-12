#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
case "${1:-all}" in
 host)
  python3 efi/tests/nested48a_core.py --sanitizer none
  python3 efi/tests/nested48a_core.py --sanitizer address,undefined
  python3 efi/tests/nested48a_core.py --sanitizer thread
  ;;
 freestanding) python3 efi/tests/nested48a_freestanding.py ;;
 all)
  bash efi/tests/nested48a.sh host
  bash efi/tests/nested48a.sh freestanding
  ;;
 *) echo 'usage: nested48a.sh [all|host|freestanding]' >&2; exit 2;;
esac
printf '%s\n' 'PASS N48A selected software proof only; N48B runtime and hardware are separate'
