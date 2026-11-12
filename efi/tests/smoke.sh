#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make smoke
set +e
timeout 90 scripts/run.sh smoke > build/smoke.log 2>&1
status=$?
set -e
cat build/smoke.log
[[ $status == 33 ]] || { echo "FAIL: QEMU exit $status, expected 33" >&2; exit 1; }
python3 - <<'PY'
from pathlib import Path
import re
text = Path('build/smoke.log').read_text(errors='replace')
markers = ['STAGE0.UEFI_ENTRY', 'IMAGE.BASE=', 'VMX.NOT_ATTEMPTED',
           'STAGE0.EXIT_BOOT_SERVICES_BEGIN', 'SYLE.KRNL_ENTRY', 'BOOT_INFO.V4', 'BOOT.MAP_BYTES=',
           'BOOT.IMAGE_BASE=', 'BOOT.ACPI_RSDP=', 'BOOT.NO_RECLAIM',
           'STAGE0.POST_EBS', 'KRNL.PMM_READY', 'KRNL.EARLY_OWN_CR3=',
           'KRNL.PAGING_RX_RO_NX_NULL_GUARD', 'STAGE0.SMOKE_PASS']
positions = [text.find(marker) for marker in markers]
assert all(pos >= 0 for pos in positions), positions
assert positions == sorted(positions), positions
assert 'ERROR.' not in text, text
values = dict(re.findall(r'([A-Z_.]+)=0x([0-9a-f]+)', text))
assert int(values['BOOT.MAP_BYTES'], 16) > 0
assert values['BOOT.IMAGE_BASE'] == values['IMAGE.BASE']
assert 'INVALID BOOT INFO' not in text
print('PASS: UEFI entry -> ExitBootServices -> standalone serial -> debug exit (TCG)')
PY
