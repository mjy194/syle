"""Offline complete logged PCI-cycle coverage; not a driver/queue completion test."""
import re,json,hashlib,pathlib,sys,collections
p=pathlib.Path(sys.argv[1]);raw=p.read_bytes();s=raw.decode().replace('\r','')
pat=r'OVMF.CLOCK_APIC pci_cycle port=(0x[0-9a-f]+)\s+q=(0x[0-9a-f]+)\s+config_address=(0x[0-9a-f]+)\s+before_rax=(0x[0-9a-f]+)\s+after_rax=(0x[0-9a-f]+)\s+accepted=(0x[0-9a-f]+)'
keys=['port','q','address','before','after','accepted']
rows=[dict(zip(keys,[int(x,16) for x in m.groups()])) for m in re.finditer(pat,s)]
assert len(rows)==s.count('OVMF.CLOCK_APIC pci_cycle port=')
for r in rows:
 assert (r['q']>>16)&65535==r['port']
 assert r['q']&7 in (0,1,3)
 r.update(width={0:1,1:2,3:4}[r['q']&7],read=bool(r['q']&8))
cfg=[r for r in rows if 0xcfc<=r['port']<=0xcff and r['address']&0x80000000]
for r in cfg:r.update(bdf=(r['address']>>8)&65535,offset=(r['address']&252)+r['port']-0xcfc)
disk=[r for r in cfg if r['bdf']==16]
print(json.dumps({'serial_sha256':hashlib.sha256(raw).hexdigest(),'cycles':len(rows),'enabled_config_cycles':len(cfg),'bdf_counts':{hex(k):v for k,v in collections.Counter(r['bdf'] for r in cfg).items()},'disk_config_cycles':disk,'disk_writes':sum(not r['read'] for r in disk),'d000_aperture_cycles':[r for r in rows if 0xd000<=r['port']<=0xdfff],'rejected_cycles':sum(r['accepted']==0 for r in rows),'last_cycles':rows[-5:],'scope':'logged cycles only; config identification is not resource assignment, driver bind, queue setup or block IO'},indent=2))
