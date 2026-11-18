#!/usr/bin/env python3
"""Verify disk -> UEFI -> Linux -> independently injected Shell commands."""
import argparse,hashlib,json,re
from pathlib import Path
COMMANDS=['uname -s\r','uname -r\r','echo SYLE_LINUX_DISK_COMMAND_OK_20261118\r']

def record(text,prefix,names):
    fields=r'\s+'.join(re.escape(name)+r'=(0x[0-9a-f]+)' for name in names)
    found=re.search(re.escape(prefix)+fields,text)
    assert found,('missing record',prefix,names)
    return dict(zip(names,(int(x,16) for x in found.groups())))

def check(raw,commands,result):
    text=raw.decode('ascii')
    tx=bytes(int(x,16) for x in re.findall(r'OVMF.CONSOLE TX byte=(0x[0-9a-f]+)',text))
    plain=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',tx).replace(b'\r',b'')
    console=record(text,'OVMF.CONSOLE terminal ',['bytes','received','reads','writes','bar','command','failed'])
    assert console['bytes']==len(tx) and not console['failed'],('partial TX capture',len(tx),console)
    assert commands==COMMANDS,commands
    cursor=0
    for marker in [b'SYLE_DISK_EFI_LOADED\n',b'SYLE_DISK_FILE_VERIFIED\n',b'SYLE_DISK_LINUX_START_IMAGE\n',
                   b'Linux version 6.12.0-syle-efi1',b'Run /init as init process\n',b'SYLE_LINUX_USERSPACE_READY\n',b'BusyBox v1.37.0']:
        pos=plain.find(marker,cursor);assert pos>=cursor,('missing/out of order',marker)
        cursor=pos+len(marker)
    # Exact command result lines exclude typed command echo as evidence.
    interaction=(b'\n# uname -s\nLinux\n# uname -r\n6.12.0-syle-efi1\n'
                 b'# echo SYLE_LINUX_DISK_COMMAND_OK_20261118\nSYLE_LINUX_DISK_COMMAND_OK_20261118\n# ')
    assert interaction in plain[cursor:],plain[-1500:]
    assert 'OVMF.DISK file_probe_complete' in text
    assert 'dispatch_rejected' not in text and 'stop_cause=' not in text and 'input_before_reservation_failed' not in text
    assert 'exit_budget_reached' not in text and 'observation_time_budget=' not in text
    trace=record(text,'OVMF.TRACE dump_end ',['failed','incomplete'])
    assert trace=={'failed':0,'incomplete':0},trace
    assert re.search(r'dropped_bytes=0x0+\s',text)
    disk=record(text,'OVMF.DISK terminal ',['bar','command','status','pfn','notifies','completions','failed','backend_failed'])
    assert disk['bar']==0xc000 and disk['command']==5 and disk['notifies']>0 and disk['completions']>0 and not disk['failed'] and not disk['backend_failed'],disk
    irq=record(text,'OVMF.DISK PIC ',['receipts','irq0','irq4','rx_sent','rx_read','active','IRR','ISR','mask'])
    count=sum(map(len,commands))
    assert irq['active']==1 and irq['irq0']>0 and irq['irq4']>0 and irq['receipts']==irq['irq0']+irq['irq4'],irq
    assert irq['rx_sent']==count and irq['rx_read']==count,irq
    assert result.get('admission_graph_equal') and result.get('continued_once') and result['returncode']==35 and not result.get('timeout') and not result.get('error'),result
    return {'validation':'PASS','acceptance':'read-only virtual ESP -> UEFI Linux EFI stub -> embedded initramfs -> interactive BusyBox Shell',
            'serial_sha256':hashlib.sha256(raw).hexdigest(),'tx_bytes':len(tx),'rx_bytes':count,'disk':disk,'irq':irq,'commands':commands,'process':result},plain

def analyze(stage):
    result,plain=check((stage/'serial.log').read_bytes(),json.loads((stage/'input.json').read_text()),json.loads((stage/'result.json').read_text()))
    (stage/'console.txt').write_bytes(plain)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('stage',type=Path);a=p.parse_args();print(json.dumps(analyze(a.stage),indent=2))
