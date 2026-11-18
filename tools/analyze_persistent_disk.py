#!/usr/bin/env python3
"""Accept disk-root Linux only after a fresh-VM restart using the same file."""
import argparse,hashlib,json,re,subprocess,tempfile
from pathlib import Path
from analyze_firmware_disk import record

def phase_check(raw,commands,result,phase,token):
    text=raw.decode('ascii');tx=bytes(int(x,16) for x in re.findall(r'OVMF.CONSOLE TX byte=(0x[0-9a-f]+)',text))
    console=record(text,'OVMF.CONSOLE terminal ',['bytes','received','reads','writes','bar','command','failed'])
    assert console['bytes']==len(tx) and not console['failed'],('incomplete TX',console,len(tx))
    plain=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',tx).replace(b'\r',b'')
    expected=[f'persistctl {phase} {token}\r','persistctl reboot\r'];assert commands==expected,commands
    cursor=0
    markers=[b'SYLE_DISK_EFI_LOADED\n',b'SYLE_DISK_FILE_VERIFIED\n',b'SYLE_DISK_LINUX_START_IMAGE\n',
             b'Linux version 6.12.0-syle-root1',b'virtio_blk virtio0: [vda]',b'vda: vda1 vda2',
             b'VFS: Mounted root (ext2 filesystem) on device 254:2.',b'Run /sbin/init as init process',
             b'SYLE_DISK_ROOT_READY\n',b'/dev/root / ext2 rw,']
    for marker in markers:
        pos=plain.find(marker,cursor);assert pos>=cursor,('missing/out of order',marker)
        cursor=pos+len(marker)
    action='WRITTEN' if phase=='write' else 'VERIFIED'
    pair=f'\n# persistctl {phase} {token}\nSYLE_PERSIST_{action} {token}\n# persistctl reboot\n'.encode()
    assert pair in plain[cursor:],plain[-2000:]
    assert b'SYLE_PERSIST_ROOT_REMOUNTED_RO\nSYLE_PERSIST_REBOOT_REQUEST\n' in plain
    assert b'reboot: machine restart' in plain
    for word in ['SYLE_PERSIST_ERROR','dispatch_rejected','stop_cause=','exit_budget_reached','observation_time_budget=']:
        assert word not in text,word
    assert 'OVMF.PERSIST guest_reset_requested backing_flushed1' in text
    assert 'OVMF.DISK file_probe_complete' in text
    assert b'pci=off' not in plain and b'Run /init as init process' not in plain
    assert record(text,'OVMF.TRACE dump_end ',['failed','incomplete'])=={'failed':0,'incomplete':0}
    assert re.search(r'dropped_bytes=0x0+\s',text)
    disk=record(text,'OVMF.PERSIST stats ',['reads','writes','flushes','irq5','reboot'])
    assert disk['reads']>0 and disk['writes']>0 and disk['flushes']>0 and disk['irq5']>0 and disk['reboot']==1,disk
    dev=record(text,'OVMF.DISK terminal ',['bar','command','status','pfn','notifies','completions','failed','backend_failed'])
    assert dev['bar']==0xc000 and dev['notifies']>0 and dev['completions']>0 and not dev['failed'] and not dev['backend_failed'],dev
    irq=record(text,'OVMF.DISK PIC ',['receipts','irq0','irq4','rx_sent','rx_read','active','IRR','ISR','mask'])
    assert irq['receipts']==irq['irq0']+irq['irq4']+disk['irq5'] and irq['irq0']>0 and irq['irq4']>0,irq
    assert irq['rx_sent']==irq['rx_read']==sum(map(len,commands)),irq
    assert result.get('admission_graph_equal') and result.get('admitted_extension') and result.get('continued_once') and result['returncode']==35 and not result.get('timeout') and not result.get('error'),result
    assert result['backing_identity']['bytes']==67108864
    return {'phase':phase,'serial_sha256':hashlib.sha256(raw).hexdigest(),'tx_bytes':len(tx),'commands':commands,'device':disk,'irq':irq,'process':result},plain

def offline(snapshot,token,expected_hash,folder):
    assert hashlib.sha256(snapshot.read_bytes()).hexdigest()==expected_hash
    folder.mkdir(parents=True,exist_ok=True);root=folder/'root.ext2'
    with snapshot.open('rb') as f:
        f.seek(75776*512);root.write_bytes(f.read((131038-75776+1)*512))
    cat=subprocess.run(['debugfs','-R','cat /persist/probe.txt',str(root)],capture_output=True,check=True)
    expected=(token+'\n').encode();assert cat.stdout==expected,cat.stdout
    fsck=subprocess.run(['e2fsck','-fn',str(root)],capture_output=True)
    assert fsck.returncode==0,fsck.stdout+fsck.stderr
    (folder/'file.txt').write_bytes(cat.stdout);(folder/'debugfs.stderr').write_bytes(cat.stderr);(folder/'fsck.log').write_bytes(fsck.stdout+fsck.stderr)
    return {'snapshot_sha256':expected_hash,'file_sha256':hashlib.sha256(cat.stdout).hexdigest(),'bytes':len(cat.stdout),'fsck_returncode':fsck.returncode}

def analyze(root):
    experiment=json.loads((root/'experiment.json').read_text());token=experiment['token'];checks=[]
    for phase,stage in [('write',root/'native/positive-once'),('read',root/'reboot/native/positive-once')]:
        result,plain=phase_check((stage/'serial.log').read_bytes(),json.loads((stage/'input.json').read_text()),json.loads((stage/'result.json').read_text()),phase,token)
        (stage/'console.txt').write_bytes(plain);checks.append(result)
    first,second=[r['process'] for r in checks]
    assert first['backing_identity']==second['backing_identity']
    assert first['qemu_pid']!=second['qemu_pid']
    assert first['backing_before_sha256']==experiment['seed_sha256']
    assert first['backing_after_sha256']==second['backing_before_sha256']
    assert first['backing_before_sha256']!=first['backing_after_sha256']
    assert token.encode() not in (root/'disk.img').read_bytes()
    config=(root/'inputs/linux.config').read_text();assert 'CONFIG_INITRAMFS_SOURCE=""' in config and 'CONFIG_EXT2_FS=y' in config
    proof=[]
    for phase,run in zip(['write','read'],[first,second]):proof.append(offline(root/f'{phase}-after.raw',token,run['backing_after_sha256'],root/'verification'/phase))
    assert proof[0]['file_sha256']==proof[1]['file_sha256']==experiment['file_sha256']
    result={'validation':'PASS','acceptance':'writable virtio-blk, Linux ext2 disk root, fsync + read-only remount, guest reset, fresh QEMU/L1/L2 restart, identical persisted file',
            'token':token,'write':checks[0],'read':checks[1],'offline':proof,'same_backing_inode':True,'kernel_has_embedded_initramfs':False}
    (root/'analysis.json').write_text(json.dumps(result,indent=2));return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);a=p.parse_args();print(json.dumps(analyze(a.root),indent=2))
