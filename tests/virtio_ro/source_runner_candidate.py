"""Generate passive runner from the already reviewed isolated guest runner.
Never runs QEMU. The resulting runner must receive independent review anew.
"""
import pathlib,sys,hashlib
src=pathlib.Path(sys.argv[1]).read_text();out=pathlib.Path(sys.argv[2])
# Pin the exact previously executed/reviewed input; no arbitrary runner ingestion.
expected=pathlib.Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(expected).hexdigest()=='d969f16c162d2ca306f07e7e8953dda045dbb4fb9423a8baa502e38151aa0467'
print('base_runner_sha256',hashlib.sha256(expected).hexdigest())
assert "query('inject-nmi')" in src and "query('cont');serial=wait_serial" in src
bootstrap="""root=pathlib.Path(__file__).resolve().parent
# Validate all frozen controls BEFORE importing candidate helpers or launching.
control_entries={}
for line in (root/'CONTROL.sha256').read_text().splitlines():
 m=re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9_.-]+)',line)
 assert m and m[2] not in control_entries,'CONTROL syntax/duplicate'
 control_entries[m[2]]=m[1]
required={'run_source_observation.py','source_page_audit.py','source_memory_policy.py','source-inventory.json','inventory-local.json','preflight-baseline.json','frozen-kernel-managers.tar.gz'}
assert required<=set(control_entries),'missing frozen control'
for name,digest in control_entries.items():
 item=root/name
 assert not item.is_symlink() and item.is_file() and hashlib.sha256(item.read_bytes()).hexdigest()==digest,('CONTROL mismatch',name)
from source_memory_policy import ram_whitelist,ram_span,new_file
allowed_ram=None
"""
assert src.count('root=pathlib.Path(__file__).resolve().parent')==1
src=src.replace('root=pathlib.Path(__file__).resolve().parent',bootstrap)
src=src.replace('assert mode in (0,1)','assert mode==0')
src=src.replace('one fresh fixture; at most one guest inject-nmi; no retry','one fresh fixture; passive source snapshot only; no injection')
src=src.replace("inv=json.loads((root/'inventory-local.json').read_text())", "inv=json.loads((root/'inventory-local.json').read_text())\nsource_inv=json.loads((root/'source-inventory.json').read_text())\nassert source_inv['kernel_sha256']==inv['kernel_sha256']\nfrom source_page_audit import Audit")
start=src.index(" old=json.loads(pathlib.Path('/home/javion/")
end=src.index(" for n in ['query-kvm'",start)
src=src[:start]+" old=json.loads((root/'preflight-baseline.json').read_text())\n"+src[end:]
start=src.index(" result['injection_attempted']=True")
end=src.index(" query('quit');result['quit_acknowledged']=True;proc.wait(timeout=5)",start)
block=''' # No resume after STOP. All subsequent commands read the same stopped fixture.
 cache={};reads=0
 def physical(pa,n):
  nonlocal_placeholder=0
  ram_span(allowed_ram,pa,n)
  pieces=[]
  while n:
   base=pa&~4095;off=pa&4095;k=min(n,4096-off)
   if base not in cache:
    assert len(cache)<8192,'physical read budget'
    paused();identity();name=p/('physical-%016x.bin'%base)
    assert not name.exists()
    query('pmemsave',{'val':base,'size':4096,'filename':str(name)})
    data=name.read_bytes();assert len(data)==4096;cache[base]=data
   pieces.append(cache[base][off:off+k]);pa+=k;n-=k
  return b''.join(pieces)
 def reg(name):
  hits=re.findall(r'(?<![A-Za-z0-9_])'+name+r'=([0-9a-fA-F]+)',regs)
  assert len(hits)==1,(name,hits)
  return int(hits[0],16)
 bound=int.from_bytes(physical(source_inv['physical_bound_address'],8),'little')
 # The frozen kernel's PMM address_limit derives from actual guest CPUID,
 # clamped down. It is a conservative validation bound, not exact MAXPHYADDR.
 audit=Audit(physical,reg('CR0'),reg('CR3'),reg('CR4'),reg('EFER'),bound,table_budget=4096)
 layout=source_inv['layout']
 for name,field,lengthfield in [('kernel','kernel','kernel_size'),('initrd','initrd','initrd_size'),('command','command_line',None)]:
  item=source_inv['sources'][name];assert u(sess,layout[field],8)==item['va']
  if lengthfield:assert u(sess,layout[lengthfield],8)==item['length']
 reports={name:audit.source(item['va'],item['length']) for name,item in source_inv['sources'].items()}
 for name,report in reports.items():assert report['sha256']==source_inv['sources'][name]['sha256']
 # Report aliases even when writable; do NOT turn a negative observation into
 # runtime permission. This snapshot never authorizes native scoped copies.
 result['source_observation']={'reports':reports,'tables':len(audit.seen),'cached_pages':len(cache),'conservative_physical_bound':bound,'scope':'current stopped Syle CR3 guest VA->QEMU GPA only; no EPT/other roots/DMA proof'}
 pathlib.Path('source-report.json').write_text(json.dumps(result['source_observation'],indent=2))
 paused();identity()
'''
src=src[:start]+block+src[end:]
src=src.replace(" snap('before')", " mtree=query('human-monitor-command',{'command-line':'info mtree -f'});pathlib.Path('stopped-mtree.txt').write_text(mtree)\n allowed_ram=ram_whitelist(mtree)\n pathlib.Path('ram-whitelist.json').write_text(json.dumps(allowed_ram))\n snap('before')")
src=src.replace("'injection_attempted':False", "'passive_source_only':True")
# Explicit read/control allowlist. No guest writes, injection, device changes.
needle=' def query(cmd,arguments=None):\n'
assert src.count(needle)==1
src=src.replace(needle,needle+"  assert cmd in {'qmp_capabilities','query-status','stop','cont','query-cpus-fast','query-kvm','query-pci','query-memory-devices','query-fdsets','human-monitor-command','pmemsave','quit'}\n  if cmd=='human-monitor-command':\n   hmp=arguments['command-line'];match=re.fullmatch(r'xp /1gx 0x([0-9a-fA-F]{1,16})',hmp)\n   assert hmp in ('cpu 0','info registers','info mtree -f') or match\n   if match:ram_span(allowed_ram,int(match[1],16),8)\n  if cmd=='pmemsave':\n   assert set(arguments)=={'val','size','filename'}\n   ram_span(allowed_ram,arguments['val'],arguments['size']);new_file(p,arguments['filename'])\n")
assert "query('inject-nmi')" not in src and 'INJECTION_ATTEMPTED' not in src
src=src.replace('  nonlocal_placeholder=0\n','')
src+='\nif result.get("error"):sys.exit(1)\n'
compile(src,str(out),'exec');out.write_text(src)
