"""Conservative read-only QMP admission; unknown flat-map text fails closed."""
import re,os,pathlib,hashlib
class Refuse(ValueError):pass
def ram_whitelist(text):
 # Inspect precisely the system address space's CURRENT flat view, not the
 # hierarchical region tree where overlapping priority windows are unresolved.
 sections=re.split(r'(?m)^FlatView #[^\n]*\n',text)
 views=[s for s in sections if re.search(r'(?m)^\s*AS "memory", root: system\s*$',s)]
 if len(views)!=1:raise Refuse('ambiguous/missing system FlatView')
 ranges=[]
 for line in views[0].splitlines():
  if '(prio ' not in line:continue
  m=re.fullmatch(r'\s*([0-9a-fA-F]{16})-([0-9a-fA-F]{16}) \(prio -?[0-9]+, (ram|rom|romd|i/o)\): (.+?)\s*',line)
  if not m:raise Refuse('unsupported FlatView region syntax')
  lo,hi=int(m[1],16),int(m[2],16)
  if hi<lo:raise Refuse('reversed range')
  ranges.append((lo,hi+1,m[3],m[4]))
 if not ranges:raise Refuse('empty FlatView')
 ranges.sort()
 if any(a[1]>b[0] for a,b in zip(ranges,ranges[1:])):raise Refuse('overlapping flat regions')
 allowed=[]
 for lo,end,kind,name in ranges:
  # QEMU 11.1.3 observed flat-view suffix KVM is an exact separate marker,
  # not part of the region name and NOT a substitute for type/overlap checks.
  match=re.fullmatch(r'pc\.ram(?: @([0-9a-fA-F]{16}))?(?: KVM)?',name)
  if kind=='ram' and match:
   offset=int(match[1],16) if match[1] is not None else 0
   if offset!=lo:raise Refuse('unsupported pc.ram backing offset')
   lo=max(lo,1<<20);end=min(end,512<<20)
   if lo<end:allowed.append((lo,end))
 if not allowed:raise Refuse('no confirmed pc.ram in conservative window')
 return allowed
def ram_span(allowed,pa,n):
 if type(pa)!=int or type(n)!=int or n<=0 or pa<1<<20 or pa>=512<<20 or n>(512<<20)-pa:raise Refuse('physical bound/type')
 end=pa+n;cursor=pa
 for lo,stop in allowed:
  if lo<=cursor<stop:cursor=min(end,stop)
  if cursor==end:return
 raise Refuse('physical span crosses unconfirmed RAM/hole')
def new_file(directory,value):
 p=pathlib.Path(value)
 if not p.is_absolute() or p.parent!=directory or not re.fullmatch(r'[A-Za-z0-9_.-]+\.bin',p.name):raise Refuse('output outside private directory')
 if os.path.lexists(p) or p.parent.resolve()!=directory:raise Refuse('output exists/symlink')
 return p
def control(root):
 entries={}
 for line in (root/'CONTROL.sha256').read_text().splitlines():
  m=re.fullmatch(r'([0-9a-f]{64})  ([A-Za-z0-9_.-]+)',line)
  if not m or m[2] in entries:raise Refuse('control manifest syntax/duplicate')
  entries[m[2]]=m[1]
 for name,digest in entries.items():
  p=root/name
  if p.is_symlink() or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:raise Refuse('control hash '+name)
 return entries
