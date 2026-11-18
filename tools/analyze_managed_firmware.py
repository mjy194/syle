#!/usr/bin/env python3
"""Verify Ring3 lifecycle, unchanged stopped state, complete release and disk data."""
import argparse,hashlib,json,re
from pathlib import Path
from analyze_persistent_disk import offline
TOKEN='9a473c02e61f8b5d7349ab1260ec572d'
def records(text,prefix,names):
    fields=r'\s+'.join(re.escape(n)+r'=(0x[0-9a-f]+)' for n in names)
    return [dict(zip(names,map(lambda x:int(x,16),m.groups()))) for m in re.finditer(re.escape(prefix)+fields,text)]
def lifecycle(raw):
    text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',raw.decode('ascii')).replace('\r','')
    for bad in ['FMG.CREATE_FAILED','FMG.STEP_FAILED','page_balance_failed','LIFECYCLE_FAILED','SYLE_PERSIST_ERROR','dispatch_rejected','stop_cause=','observation_time_budget=','mount:']:
        assert bad not in text,bad
    assert 'FMG.BACKEND_INSTALLED fixed_UEFI_writable_disk_profile1' in text
    assert 'MGMT.ELF_LOADED' in text and 'RING3' in text
    assert text.count('\nSYLE_DISK_ROOT_READY\n')==2
    assert text.count('\nSYLE_LIFE_ARMED\n')==1
    assert text.count('\nSYLE_LIFE_RESUMED\n')==1
    assert text.count('\nSYLE_LIFE_RECREATED\n')==1
    assert text.count('\nSYLE_PERSIST_WRITTEN '+TOKEN+'\n')==1
    assert text.count('\nSYLE_PERSIST_VERIFIED '+TOKEN+'\n')==2
    order=['FMG.CREATE generation=0x0000000000000001','\nSYLE_LIFE_ARMED\n','FMG.STOP ', 'FMG.RESUME ', '\nSYLE_LIFE_RESUMED\n','FMG.RELEASE ', 'FMG.CREATE generation=0x0000000000000002', '\nSYLE_LIFE_RECREATED\n','FMG.RELEASE ', 'SYLE.MGMT FIRMWARE_LIFECYCLE_PASS','MGMT.EXIT status=0x0000000000000000','KRNL.IDLE']
    cursor=0
    for marker in order:
        i=text.find(marker,cursor);assert i>=cursor,marker;cursor=i+len(marker)
    creates=records(text,'FMG.CREATE ',['generation','vmcs','ept','allocated_delta'])
    stops=records(text,'FMG.STOP ',['generation','entries','state_hash'])
    resumes=records(text,'FMG.RESUME same_guest=1 ',['generation','entries_unchanged','state_hash'])
    releases=records(text,'FMG.RELEASE ',['generation','pages_baseline','pages_after','vmx_active','vmxon','vmcs','ept_released','adapter_revoked','reads','writes','flushes','irq5','rx_sent','rx_delivered','tx_produced','tx_received','trace_failed','trace_dropped','trace_encoded'])
    assert len(creates)==2 and len(stops)==3 and len(resumes)==1 and len(releases)==2
    assert [c['generation'] for c in creates]==[1,2] and all(c['vmcs'] and c['ept'] and c['allocated_delta']>32768 for c in creates)
    assert resumes[0]==dict(generation=stops[0]['generation'],entries_unchanged=stops[0]['entries'],state_hash=stops[0]['state_hash'])
    assert [s['generation'] for s in stops]==[1,1,2] and stops[1]['entries']>stops[0]['entries']
    for gen,r in enumerate(releases,1):
        assert r['generation']==gen and r['pages_baseline']==r['pages_after']==releases[0]['pages_baseline']
        assert r['vmx_active']==r['vmxon']==r['vmcs']==r['trace_failed']==r['trace_dropped']==0
        assert r['ept_released']==r['adapter_revoked']==1
        assert r['reads']>0 and r['writes']>0 and r['flushes']>0 and r['irq5']>0
        assert r['rx_sent']==r['rx_delivered']>0 and r['tx_produced']==r['tx_received']>0
    assert text.count('attempting_L1_WBINVD')==2
    return {'creates':creates,'stops':stops,'resumes':resumes,'releases':releases,'architectural_wbinvd_observed':2},text

def analyze(root):
    checks,text=lifecycle((root/'native/positive-once/serial.log').read_bytes())
    run=json.loads((root/'native/positive-once/result.json').read_text())
    assert run['returncode']==33 and run['continued_once'] and run['admission_graph_equal'] and run['admitted_extension']
    assert not run.get('timeout') and not run.get('error')
    assert run['backing_identity']['bytes']==67108864 and run['backing_identity']['path']==str((root/'backing.img').resolve())
    plan=json.loads((root/'native/lifecycle-plan.json').read_text());assert plan['host_serial_input']==[]
    assert not (root/'native/positive-once/input.json').exists()
    seed=(root/'disk.img').read_bytes();assert TOKEN.encode() not in seed
    assert hashlib.sha256(seed).hexdigest()==run['backing_before_sha256']==plan['baseline_backing_sha256']
    assert run['backing_after_sha256']!=run['backing_before_sha256']
    for name in ['tiny','nohv']:
        stage=root/'native'/name;r=json.loads((stage/'result.json').read_text());s=(stage/'serial.log').read_text()
        assert r['qemu_returncode']==(33 if name=='tiny' else 35) and not r.get('timeout')
        assert ('FIRMWARE.PROBE PASS reset16->protected32->long64 cleanup' if name=='tiny' else 'OVMF.CLOCK_APIC source_admission_failed') in s
        assert 'attempting_L1_WBINVD' not in s
    for line in (root/'source.sha256').read_text().splitlines():
        digest,name=line.split(None,1);assert hashlib.sha256((root/'source'/name.strip()).read_bytes()).hexdigest()==digest,name
    for kind in ['tiny','nohv','positive-once']:
        stage=root/'native'/kind
        for line in (stage/'media.sha256').read_text().splitlines():
            digest,name=line.split(None,1)
            # VARS is deliberately writable firmware state during the native run.
            if name.strip()!='vars.fd':assert hashlib.sha256((stage/name.strip()).read_bytes()).hexdigest()==digest,name
    file=offline(root/'backing.img',TOKEN,run['backing_after_sha256'],root/'verification')
    result={'validation':'PASS','acceptance':'Ring3 managed UEFI + writable disk: same-guest STOP/START, PID and variable retained, full page balance after both DESTROY, new guest reads persisted file','checks':checks,'run':run,'offline':file,'source_manifest_verified':True,'host_serial_input':[],'trace_policy':'bounded selected trace retained only until release; aggregate integrity counters, no full trace serialization'}
    (root/'analysis.json').write_text(json.dumps(result,indent=2));(root/'native/positive-once/console-and-host.txt').write_text(text)
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);args=p.parse_args();print(json.dumps(analyze(args.root),indent=2))
