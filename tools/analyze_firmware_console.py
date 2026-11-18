#!/usr/bin/env python3
"""Check actual firmware TX and independently recorded host RX commands."""
import argparse,hashlib,json,re
from pathlib import Path

def analyze(stage):
    raw=(stage/'serial.log').read_bytes();text=raw.decode('ascii')
    tx=bytes(int(x,16) for x in re.findall(r'OVMF.CONSOLE TX byte=(0x[0-9a-f]+)',text))
    plain=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',tx).replace(b'\r',b'')
    commands=json.loads((stage/'input.json').read_text())
    assert commands==['ver\r','echo SYLE_CONSOLE_OK_20261117\r'],commands
    assert b'UEFI Interactive Shell v2.2' in plain
    # Require separate command result, not only the echoed command token.
    assert re.search(rb'\nSYLE_CONSOLE_OK_20261117\s*\n',plain),plain[-1200:]
    assert plain.count(b'Shell>')>=3
    first=plain.index(b'Shell>');second=plain.index(b'Shell>',first+6)
    assert b'UEFI v' in plain[first:second] or b'UEFI Shell version' in plain[first:second],plain[first:second]
    assert 'OVMF.CONSOLE command_probe_complete' in text
    terminal=re.search(r'OVMF.CONSOLE terminal bytes=(0x[0-9a-f]+)\s+received=(0x[0-9a-f]+)\s+reads=(0x[0-9a-f]+)\s+writes=(0x[0-9a-f]+)\s+bar=(0x[0-9a-f]+)\s+command=(0x[0-9a-f]+)\s+failed=(0x[0-9a-f]+)',text.replace('\r','').replace('\n\n','\n'))
    assert terminal,'missing terminal'
    values=[int(x,16) for x in terminal.groups()];assert values[0]==len(tx) and values[1]==sum(map(len,commands)) and values[5]==1 and values[6]==0,values
    assert 'dispatch_rejected' not in text and 'compact trace overflow' not in text
    assert re.search(r'OVMF.TRACE dump_end failed=0x0+\s+incomplete=0x0+',text)
    result=json.loads((stage/'result.json').read_text());assert result.get('admission_graph_equal') and result.get('continued_once') and result['returncode']==35 and not result.get('timeout') and not result.get('error'),result
    (stage/'console.txt').write_bytes(plain)
    return {'validation':'PASS','serial_sha256':hashlib.sha256(raw).hexdigest(),'tx_bytes':len(tx),'rx_bytes':values[1],'bar':hex(values[4]),'commands':commands,'transcript':plain.decode(),'process':result}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('stage',type=Path);a=p.parse_args();print(json.dumps(analyze(a.stage),indent=2))
