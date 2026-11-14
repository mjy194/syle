"""Read-only Syle-current-CR3 audit. QEMU guest VA -> QEMU guest PA, NOT EPT.
Unsupported/ambiguous tables throw Inconclusive; no global-alias safety claim.
"""
import hashlib,struct
class Inconclusive(ValueError): pass
U64=(1<<64)-1
NX=1<<63
class Audit:
 def __init__(self,read,cr0,cr3,cr4,efer,physical_bound,table_budget=4096,allow_1g=False):
  # physical_bound is captured allocator limit derived from CPUID, possibly
  # clamped DOWN. It is a conservative address bound, not exact MAXPHYADDR.
  if physical_bound<1<<32 or physical_bound>1<<52 or physical_bound&(physical_bound-1):raise Inconclusive('physical bound')
  if cr0&(1<<31|1<<16|1)!=(1<<31|1<<16|1):raise Inconclusive('PG/WP/PE required')
  if not cr4&(1<<5) or cr4&((1<<12)|(1<<17)|(1<<22)|(1<<24)):raise Inconclusive('unsupported LA57/PCIDE/PKE/PKS or no PAE')
  if efer&((1<<10)|(1<<11))!=((1<<10)|(1<<11)):raise Inconclusive('LMA/NXE required')
  if cr3&~((physical_bound-1)&~4095 | 0x18):raise Inconclusive('unsupported CR3 bits')
  self.read=read;self.bound=physical_bound;self.mask=(physical_bound-1)&~4095
  self.allow_1g=allow_1g;self.budget=table_budget;self.seen=set();self.leaves=[]
  root=cr3&self.mask
  if not root:raise Inconclusive('null root')
  self.walk(root,39,0,True,True,False)
 def data(self,pa,n):
  if pa<0 or n<0 or pa>self.bound or n>self.bound-pa:raise Inconclusive('physical span')
  b=self.read(pa,n)
  if len(b)!=n:raise Inconclusive('truncated memory')
  return b
 def walk(self,pa,shift,prefix,rw,us,nx):
  if pa in self.seen:raise Inconclusive('repeated/cyclic table: cannot skip alternate VA/permission path')
  if len(self.seen)>=self.budget:raise Inconclusive('table budget')
  self.seen.add(pa)
  for i,e in enumerate(struct.unpack('<512Q',self.data(pa,4096))):
   if not e&1:continue
   if e&~(self.mask|0xfff|NX):raise Inconclusive('unsupported/reserved entry bits')
   nrw=rw and bool(e&2);nus=us and bool(e&4);nnx=nx or bool(e&NX)
   va=prefix|(i<<shift)
   if shift==39 and e&128:raise Inconclusive('PS at PML4')
   leaf=shift==12 or bool(e&128)
   if leaf:
    if shift==30 and not self.allow_1g:raise Inconclusive('1 GiB pages require independent CPU feature evidence')
    size=1<<shift;raw=e&self.mask
    if shift>12 and raw&((size-1)&~4096):raise Inconclusive('misaligned huge leaf')
    base=raw&~(size-1)
    if base>self.bound or size>self.bound-base:raise Inconclusive('leaf physical end')
    if va&(1<<47):va|=U64^((1<<48)-1)
    self.leaves.append((va,size,base,nrw,nus,nnx))
   else:
    child=e&self.mask
    if not child:raise Inconclusive('null child')
    self.walk(child,shift-9,va,nrw,nus,nnx)
 def source(self,va,n):
  if not n or va<0 or va>U64 or n-1>U64-va:raise Inconclusive('source virtual span')
  left=n;cursor=va;digest=hashlib.sha256();segments=[]
  while left:
   found=[x for x in self.leaves if x[0]<=cursor and cursor-x[0]<x[1]]
   if len(found)!=1:raise Inconclusive('source translation missing/ambiguous')
   v,size,p,rw,us,nx=found[0]
   if rw or us or not nx:raise Inconclusive('source not effective supervisor RO NX')
   k=min(left,size-(cursor-v));physical=p+cursor-v
   digest.update(self.data(physical,k));segments.append((physical,k));cursor+=k;left-=k
  aliases=[]
  for v,size,p,rw,us,nx in self.leaves:
   if any(p<a+k and a<p+size for a,k in segments):
    aliases.append({'va':v,'size':size,'pa':p,'rw':rw,'user':us,'nx':nx})
  return {'va':va,'length':n,'sha256':digest.hexdigest(),'physical_segments':segments,'reachable_aliases':aliases,'writable_alias_observed':any(x['rw'] for x in aliases),'scope':'single stopped current CR3 snapshot only; not EPT/other CR3/DMA'}
