#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT HUP INT TERM
"$here/build.sh" "$t/probe"
for mode in clock0 clock10 clock100 clock1000 clockspin; do
 "$t/probe" "$mode" > "$t/$mode"
done
python3 - "$t" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
for mode in ['clock0','clock10','clock100','clock1000','clockspin']:
 text=(root/mode).read_text().splitlines()
 assert text[1]=='SYLE_CLOCK_BEGIN '+mode
 assert text[2]=='SYLE_CLOCK_END '+mode
 assert text[-1]=='SYLE_CLOCK_DONE '+mode
 data=dict(line.split('=',1) for line in text[3:-1])
 assert set(data)=={'mono_ns','raw_ns','tsc_delta'}
 assert all(int(v)>=0 for v in data.values())
 if mode not in ['clock0','clockspin']:
  # Test on the ordinary host only; this is not a guest-time assertion.
  assert int(data['mono_ns']) >= int(mode[5:])*1000000
print('PASS independent clock sampling/output protocol (host only)')
PY
