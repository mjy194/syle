"""Offline captured-byte check. Never launches or connects to a VM."""
import pathlib,json,sys,re,subprocess
root=pathlib.Path(sys.argv[1]); mode=root/'mode0'
out=json.loads(subprocess.check_output([sys.executable,str(pathlib.Path(__file__).with_name('verify_source_capture.py')),str(root)]))
assert out['outcome']=='offline-replay-matches'
inv=json.loads((root/'inventory-local.json').read_text()); layout=inv['layout']
regs=(mode/'before-registers.txt').read_text()
def reg(n):
 m=re.findall(r'\b'+n+r'\s*=([0-9a-fA-F]+)',regs);assert len(m)==1;return int(m[0],16)
assert reg('CS')==8 and reg('SS')==16 and not reg('RFL')&0x600
assert 'CPL=0' in regs and 'SMM=0' in regs
assert reg('RIP') in inv['prepare_window']['instruction_rips']
sess=(mode/'before-session.bin').read_bytes();mg=(mode/'before-managed_guest.bin').read_bytes()
assert len(sess)==layout['session.size'] and len(mg)==layout['managed_guest.size']
def field(data,key,n):
 off=layout[key];assert off+n<=len(data);return int.from_bytes(data[off:off+n],'little')
checks={k:field(mg,'managed_guest.'+k,n) for k,n in [('state',4),('handle',8),('busy',1)]}
assert checks=={'state':1,'handle':1,'busy':1}
assert field(sess,'session.configured',1)==1
for k in ['prepared','enabled','host.active','memory.prepared','state.ready']:
 assert field(sess,'session.'+k,1)==0
for k in ['memory.eptp','host.vmxon_page','host.vmcs_page']: assert field(sess,'session.'+k,8)==0
for k,count in [('pages',32768),('tables',67)]:
 off=layout['session.memory.'+k];assert not any(sess[off:off+count*8])
pa=inv['prepare_window']['paging_root_address'];data=(mode/('physical-%016x.bin'%(pa&~4095))).read_bytes();off=pa&4095
assert off<=4088 and int.from_bytes(data[off:off+8],'little')==reg('CR3')
out.update(prepare_context=checks,rip=hex(reg('RIP')),paging_root_matches=True,all_session_ownership_empty=True,configured=True,scope='one stopped pre-lgs_prepare kernel-root snapshot; no lasting ownership, other roots, DMA or native map authorization')
print(json.dumps(out,indent=2))
