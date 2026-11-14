"""Offline logged-cycle audit; no claims about unlogged accesses or module cache."""
import re,json,hashlib,pathlib,sys
p=pathlib.Path(sys.argv[1]);raw=p.read_bytes();s=raw.decode(errors='strict').replace('\r','')
pat=r'OVMF.CLOCK_APIC pci_cycle port=(0x[0-9a-f]+)\s+q=(0x[0-9a-f]+)\s+config_address=(0x[0-9a-f]+)\s+before_rax=(0x[0-9a-f]+)\s+after_rax=(0x[0-9a-f]+)\s+accepted=(0x[0-9a-f]+)\s+OVMF.CLOCK_APIC pm_state base=(0x[0-9a-f]+)\s+enabled=(0x[0-9a-f]+)\s+ticks=(0x[0-9a-f]+)\s+status=(0x[0-9a-f]+)'
keys=['port','q','address','before','after','accepted','pmba','enabled','ticks','status'];rows=[dict(zip(keys,[int(x,16) for x in m.groups()])) for m in re.finditer(pat,s)]
assert len(rows)==s.count('OVMF.CLOCK_APIC pci_cycle port=')
selected=[];seen=set();timer=0
for i,r in enumerate(rows):
 size={0:1,1:2,3:4}[r['q']&7];inp=bool(r['q']&8);mask=(1<<(size*8))-1;kind=None
 if r['port']==0xcfe and r['address']==0x80000000 and inp:kind='hostbridge_did';assert r['after']&mask==0x1237
 if r['port']==0xcfc and r['address']==0x80000b80:kind='pm_enable_read' if inp else 'pm_enable_write'
 if r['port']==0xcfc and r['address']==0x80000b40:kind='pmba_read' if inp else 'pmba_write'
 if r['port']==0xb008:kind='timer_read';timer+=1;assert inp and size==4 and r['accepted']==1 and r['pmba']==0xb001 and r['enabled']==1 and r['after']==(r['ticks']&0xffffff)
 if kind and kind not in seen:
  seen.add(kind);selected.append(dict(index=i,kind=kind,width=size,value=hex((r['after'] if inp else r['before'])&mask),accepted=r['accepted'],pmba=hex(r['pmba']),enabled=r['enabled']))
assert {'hostbridge_did','pm_enable_read','pm_enable_write','pmba_read','pmba_write','timer_read'}<=seen
print(json.dumps({'serial_sha256':hashlib.sha256(raw).hexdigest(),'cycles':len(rows),'timer_reads_verified':timer,'command04_samples':sum(r['address']==0x80000b04 for r in rows),'first_of_each_kind':selected,'scope':'logged pinned-firmware observed IO only; source interpretation does not identify loaded module or prove caching/absence of unlogged accesses'},indent=2))
