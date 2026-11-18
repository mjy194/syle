#!/usr/bin/env python3
"""Reject incomplete evidence and a result inferred from command echo alone."""
import argparse,json,tempfile
from pathlib import Path
from analyze_firmware_console import analyze
p=argparse.ArgumentParser();p.add_argument('stage',type=Path);a=p.parse_args()
valid=analyze(a.stage);rejected=[]
for name in ['missing_tx','missing_input','missing_footer','missing_probe','process_timeout']:
    with tempfile.TemporaryDirectory() as tmp:
        stage=Path(tmp)
        for f in ['serial.log','input.json','result.json']:(stage/f).write_bytes((a.stage/f).read_bytes())
        if name=='missing_input':(stage/'input.json').write_text('["ver\\r"]')
        elif name=='process_timeout':
            v=json.loads((stage/'result.json').read_text());v['timeout']=True;(stage/'result.json').write_text(json.dumps(v))
        else:
            b=(stage/'serial.log').read_bytes();target={'missing_tx':b'OVMF.CONSOLE TX byte=','missing_footer':b'OVMF.TRACE dump_end','missing_probe':b'OVMF.CONSOLE command_probe_complete'}[name]
            (stage/'serial.log').write_bytes(b.replace(target,b'INVALID_EVIDENCE'))
        try:analyze(stage)
        except (AssertionError,ValueError):rejected.append(name)
        else:raise AssertionError('accepted damaged evidence: '+name)
print(json.dumps({'valid_log':'PASS','rejected_mutations':rejected},indent=2))
