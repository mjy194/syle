#!/usr/bin/env python3
"""Isolated LGM operation seam. No edits to default source."""
import pathlib,sys
s=pathlib.Path(sys.argv[1]).read_text()
def sub(a,b):
 global s
 assert s.count(a)==1,(a,s.count(a));s=s.replace(a,b)
sub('#include "linux_guest_memory.h"','#include "lgm_bounded_candidate.h"')
sub('static void*owned(', 'static bool owned(')
sub(' void*p=o->map(o->context,*token);\n if(!p||((uintptr_t)p&7))return 0;\n wipe(p,4096);return p;', ' return cb_page_zero(o->context,*token,role,0,4096);')
a=s.index('static void*map(');b=s.index('static bool prepare(',a)
s=s[:a]+'''static bool copy(void*ctx,enum cs_source source,uint64_t gpa,const void*src,size_t n){
 struct linux_guest_memory*m=ctx;
 struct vbs span;if(!vbs_init(&span,LGM_RAM_SIZE,gpa,n)||(!src&&n))return false;
 uint64_t page;size_t off,k,done=0;
 while(vbs_next(&span,&page,&off,&k)){
  if(!cb_page_write(m->owner.context,m->pages[page],SG_MEMORY_RAM,off,source,(const unsigned char*)src+done,k))return false;
  done+=k;
 }return done==n;
}
static bool zero(void*ctx,uint64_t gpa,size_t n){
 struct linux_guest_memory*m=ctx;
 struct vbs span;if(!vbs_init(&span,LGM_RAM_SIZE,gpa,n))return false;
 uint64_t page;size_t off,k,done=0;
 while(vbs_next(&span,&page,&off,&k)){
  if(!cb_page_zero(m->owner.context,m->pages[page],SG_MEMORY_RAM,off,k))return false;
  done+=k;
 }return done==n;
}
''' + s[b:]
sub('||!o->map||!o->writeback','||!o->writeback')
sub('struct lb_writer writer={m,copy,zero};','struct cs_writer writer={m,copy,zero};')
a=s.index(' for(unsigned i=0;i<2;i++){');b=s.index(' m->plan=plan;',a)
s=s[:a]+''' for(unsigned i=0;i<2;i++)if(!cb_page_u64(m->owner.context,m->tables[i],SG_MEMORY_EPT,0,m->tables[i+1]|7))goto fail;
 for(unsigned i=0;i<LGM_PT_COUNT;i++){
  if(!cb_page_u64(m->owner.context,m->tables[2],SG_MEMORY_EPT,i*8,m->tables[3+i]|7))goto fail;
  for(unsigned j=0;j<512;j++)if(!cb_page_u64(m->owner.context,m->tables[3+i],SG_MEMORY_EPT,j*8,m->pages[i*512+j]|0x37))goto fail;
 }
''' + s[b:]
for name in ['lb_inspect','lb_load','lb_entry_prepare']:
 s=s.replace(name,'cs_'+name)
for name in ['lgm_prepare_roles','lgm_prepare','lgm_release']:
 # order longest first; avoid replacing existing generated prefixes again
 import re
 s=re.sub(r'\b'+name+r'\b','cb_'+name,s)
assert '->map(' not in s and 'static void*' not in s
pathlib.Path(sys.argv[2]).write_text(s)
