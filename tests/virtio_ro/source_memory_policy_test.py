import unittest,tempfile,pathlib,hashlib
from source_memory_policy import *
VIEW='''FlatView #0
 AS "memory", root: system
 Root memory region: system
 0000000000000000-000000000009ffff (prio 0, ram): pc.ram
 00000000000a0000-00000000000bffff (prio 0, i/o): vga
 0000000000100000-0000000007ffffff (prio 0, ram): pc.ram @0000000000100000
 0000000008000000-00000000080fffff (prio 0, i/o): hole
 0000000008100000-000000001fffffff (prio 0, ram): pc.ram @0000000008100000
FlatView #1
 AS "I/O", root: io
 0000000000000000-000000000000ffff (prio 0, i/o): io
'''
class Test(unittest.TestCase):
 def test_holes(self):
  a=ram_whitelist(VIEW);ram_span(a,0x100000,4096);ram_span(a,0x7fff000,4096)
  for p,n in [(0xa0000,4096),(0xfffff,1),(0x8000000,4096),(0x7fff000,8192),(0x20000000,1),(0x100000,1<<64)]:
   with self.assertRaises(Refuse):ram_span(a,p,n)
 def test_map_reject(self):
  for s in [VIEW+VIEW,VIEW.replace('(prio 0, ram)','(prio 0, unknown)',1),VIEW.replace('0000000008100000-','0000000008000000-')]:
   with self.assertRaises(Refuse):ram_whitelist(s)
 def test_paths_hash(self):
  with tempfile.TemporaryDirectory() as t:
   root=pathlib.Path(t);p=root/'new.bin';self.assertEqual(new_file(root,str(p)),p)
   p.write_bytes(b'x')
   with self.assertRaises(Refuse):new_file(root,str(p))
   q=root/'dangling.bin';q.symlink_to(root/'absent')
   with self.assertRaises(Refuse):new_file(root,str(q))
   with self.assertRaises(Refuse):new_file(root,str(root/'../outside.bin'))
   (root/'CONTROL.sha256').write_text(hashlib.sha256(b'x').hexdigest()+'  new.bin\n');control(root)
   p.write_bytes(b'y')
   with self.assertRaises(Refuse):control(root)
 def test_actual_qemu_flatview(self):
  path=pathlib.Path(__file__).parent/'evidence/source-qemu-11.1.3-flatview.txt'
  if not path.exists():path=pathlib.Path(__file__).parent/'source-qemu-11.1.3-flatview.txt'
  raw=path.read_bytes();self.assertEqual(hashlib.sha256(raw).hexdigest(),'550660bcd6f4b376441d30ff661849b7fcc372c712c03e79f0b6960bc718794c')
  text=raw.decode();a=ram_whitelist(text);self.assertEqual(a,[(1<<20,512<<20)])
  for pa in (0xa0000,0xc0000,0x80000000,0x380000000000):
   with self.assertRaises(Refuse):ram_span(a,pa,4096)
  ram_span(a,0x4000000,4096)
  before,system=text.rsplit('FlatView #3',1)
  for changed in [system.replace('pc.ram @0000000000100000 KVM','pc.ram @0000000000100000 KVM-extra'),system.replace('pc.ram @0000000000100000 KVM','pc.ram @0000000000200000 KVM'),system.replace('(prio 0, ram): pc.ram @0000000000100000 KVM','(prio 0, i/o): pc.ram @0000000000100000 KVM'),system+'  0000000000200000-0000000000200fff (prio 1, i/o): overlap\n']:
   with self.assertRaises(Refuse):ram_whitelist(before+'FlatView #3'+changed)
if __name__=='__main__':unittest.main()
