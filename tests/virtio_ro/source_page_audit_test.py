import struct,unittest,hashlib
from source_page_audit import Audit,Inconclusive,NX
class Test(unittest.TestCase):
 def setUp(self):
  self.mem={};self.put(0x1000,0,0x2003);self.put(0x2000,0,0x3003);self.put(0x3000,0,0x4003);self.put(0x4000,5,0x9001|NX);self.mem[0x9000]=bytearray(b'A'*4096)
 def put(self,p,i,e):
  b=self.mem.setdefault(p,bytearray(4096));struct.pack_into('<Q',b,i*8,e)
 def read(self,p,n):
  out=b''
  while n:
   page=p&~4095;k=min(n,4096-(p&4095));out+=bytes(self.mem.get(page,b'')[p&4095:(p&4095)+k]);p+=k;n-=k
  return out
 def audit(self,**kw):
  d=dict(read=self.read,cr0=(1<<31)|(1<<16)|1,cr3=0x1000,cr4=1<<5,efer=(1<<10)|(1<<11),physical_bound=1<<36);d.update(kw);return Audit(**d)
 def test_partial_cross_page(self):
  self.put(0x4000,6,0xa001|NX);self.mem[0xa000]=bytearray(b'B'*4096)
  r=self.audit().source(0x5fff,2);self.assertEqual(r['sha256'],hashlib.sha256(b'AB').hexdigest())
 def test_parent_ro(self):
  self.put(0x1000,0,0x2001);self.put(0x4000,5,0x9003|NX);self.assertFalse(self.audit().source(0x5001,2)['writable_alias_observed'])
 def test_alias(self):
  self.put(0x4000,6,0x9003|NX);self.assertTrue(self.audit().source(0x5001,2)['writable_alias_observed'])
 def test_repeated_cycle(self):
  self.put(0x1000,1,0x2003)
  with self.assertRaises(Inconclusive):self.audit()
  self.put(0x1000,1,0);self.put(0x2000,0,0x1003)
  with self.assertRaises(Inconclusive):self.audit()
 def test_budget_truncated(self):
  with self.assertRaises(Inconclusive):self.audit(table_budget=2)
  self.mem[0x3000]=bytearray(7)
  with self.assertRaises(Inconclusive):self.audit()
 def test_modes(self):
  for key,value in [('cr4',(1<<5)|(1<<12)),('cr4',(1<<5)|(1<<17)),('cr0',1<<31|1),('efer',1<<10),('cr3',0x1001)]:
   with self.assertRaises(Inconclusive):self.audit(**{key:value})
 def test_huge(self):
  self.put(0x2000,1,0x40000081|NX);self.put(0x3000,1,0x200081|NX)
  a=self.audit(allow_1g=True);self.assertEqual(len(a.leaves),3)
  self.put(0x3000,1,0x202081|NX)
  with self.assertRaises(Inconclusive):self.audit()
 def test_reserved(self):
  self.put(0x4000,5,(1<<45)|0x9001|NX)
  with self.assertRaises(Inconclusive):self.audit()
 def test_permissions(self):
  self.put(0x4000,5,0x9003|NX)
  with self.assertRaises(Inconclusive):self.audit().source(0x5000,1)
 def test_user_and_nx_accumulation(self):
  self.put(0x1000,0,0x2007|NX);self.put(0x2000,0,0x3007);self.put(0x3000,0,0x4007);self.put(0x4000,5,0x9005)
  with self.assertRaises(Inconclusive):self.audit().source(0x5000,1)
  self.put(0x2000,0,0x3003)
  self.assertTrue(self.audit().source(0x5000,1)['reachable_aliases'][0]['nx'])
 def test_high_canonical(self):
  self.put(0x1000,0,0);self.put(0x1000,511,0x2003)
  r=self.audit().source(0xffffff8000005000,1);self.assertEqual(r['physical_segments'],[(0x9000,1)])
 def test_default_1g_refused(self):
  self.put(0x2000,1,0x40000081|NX)
  with self.assertRaises(Inconclusive):self.audit()
if __name__=='__main__':unittest.main()
