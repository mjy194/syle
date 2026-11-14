#!/bin/sh
set -eu
cd "$(dirname "$0")"
for defs in '' '-DSYLE_FPM_SCI_CONTROL'; do
 cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined $defs test.c firmware_piix_pm.c firmware_pmtimer.c firmware_pci_probe.c firmware_cpuhp_absent.c firmware_port92.c -o test-bin
 ASAN_OPTIONS=detect_leaks=0 ./test-bin
done
