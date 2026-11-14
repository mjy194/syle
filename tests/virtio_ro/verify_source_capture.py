"""Offline replay only; never connects to QMP or launches anything.
Usage: python verify_source_capture.py EXTRACTED_CANDIDATE_ROOT
"""
import pathlib,json,re,hashlib,sys
from source_page_audit import Audit
from source_memory_policy import ram_whitelist,ram_span
root=pathlib.Path(sys.argv[1]);mode=root/'mode0';inv=json.loads((root/'source-inventory.json').read_text());result=json.loads((mode/'result.json').read_text())
req=[json.loads(x)['request'] for x in (mode/'qmp.jsonl').read_text().splitlines() if 'request' in json.loads(x)]
assert not any(x['execute']=='inject-nmi' for x in req)
assert sum(x['execute']=='cont' for x in req)==1
assert result['reaped']
if 'error' in result:
 print(json.dumps({'outcome':'INCONCLUSIVE','recorded_error':result['error'],'pmemsave_count':sum(x['execute']=='pmemsave' for x in req),'scope':'no successful source observation claimed'},indent=2));sys.exit(0)
allowed=ram_whitelist((mode/'stopped-mtree.txt').read_text());regs=(mode/'before-registers.txt').read_text()
def reg(n):
 hits=re.findall(r'(?<![A-Za-z0-9_])'+n+r'=([0-9a-fA-F]+)',regs);assert len(hits)==1;return int(hits[0],16)
def read(pa,n):
 ram_span(allowed,pa,n);out=[]
 while n:
  base=pa&~4095;off=pa&4095;k=min(n,4096-off);p=mode/('physical-%016x.bin'%base)
  assert not p.is_symlink();data=p.read_bytes();assert len(data)==4096
  out.append(data[off:off+k]);pa+=k;n-=k
 return b''.join(out)
seen=set()
for request in req:
 if request['execute']=='pmemsave':
  a=request['arguments'];ram_span(allowed,a['val'],a['size']);name=pathlib.Path(a['filename']).name
  assert name not in seen;seen.add(name);p=mode/name;assert not p.is_symlink() and p.stat().st_size==a['size']
  if name.startswith('physical-'):assert int(name[9:-4],16)==a['val'] and a['size']==4096
 elif request['execute']=='human-monitor-command':
  command=request['arguments']['command-line'];m=re.fullmatch(r'xp /1gx 0x([0-9a-fA-F]{1,16})',command)
  assert command in ('cpu 0','info registers','info mtree -f') or m
  if m:ram_span(allowed,int(m[1],16),8)
bound=int.from_bytes(read(inv['physical_bound_address'],8),'little')
a=Audit(read,reg('CR0'),reg('CR3'),reg('CR4'),reg('EFER'),bound)
sess=(mode/'before-session.bin').read_bytes();assert len(sess)==inv['session_size']
reports={}
for name,field,sizefield in [('kernel','kernel','kernel_size'),('initrd','initrd','initrd_size'),('command','command_line',None)]:
 item=inv['sources'][name];offset=inv['layout'][field];assert int.from_bytes(sess[offset:offset+8],'little')==item['va']
 if sizefield:
  offset=inv['layout'][sizefield];assert int.from_bytes(sess[offset:offset+8],'little')==item['length']
 r=a.source(item['va'],item['length']);assert r['sha256']==item['sha256'];reports[name]=r
observed=json.loads((mode/'source-report.json').read_text())
# Normalize tuples to JSON arrays before comparing independently recomputed data.
assert json.loads(json.dumps(reports))==observed['reports']
assert observed['tables']==len(a.seen)
assert result.get('quit_acknowledged') and result['returncode']==0
print(json.dumps({'outcome':'offline-replay-matches','tables':len(a.seen),'pmemsave_count':len(seen),'cr0':hex(reg('CR0')),'cr3':hex(reg('CR3')),'cr4':hex(reg('CR4')),'efer':hex(reg('EFER')),'physical_bound':bound,'sources':{n:{'length':r['length'],'sha256':r['sha256'],'reachable_aliases':len(r['reachable_aliases']),'writable_alias_observed':r['writable_alias_observed']} for n,r in reports.items()},'scope':'single stopped current Syle CR3 only; not EPT, other roots, dynamic lifetime or DMA'},indent=2))
