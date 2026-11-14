#!/usr/bin/env python3
"""Isolated, exact-count transformation; never edits the shared boot producer."""
import pathlib,sys
src=pathlib.Path(sys.argv[1]).read_text()
old='w->write(w->context,'
assert src.count(old)==12
src=src.replace('"linux_boot.h"','"boot_source_candidate.h"')
for name in ['lb_inspect','lb_load','lb_entry_prepare']:
 src=src.replace(name,'cs_'+name)
src=src.replace('struct lb_writer','struct cs_writer')
# The four load producer callsites have explicit provenance. All remaining
# entry_prepare calls produce q[8] (one final 2-byte store).
for expr,kind in [('p.kernel_addr,b+p.payload_offset,p.kernel_size','CS_IMAGE'),('p.initrd_addr,initrd,rd','CS_INITRD'),('p.params_addr,bp,sizeof bp','CS_PARAMS'),('p.cmdline_addr,cmd,p.cmdline_size','CS_CMD')]:
 target=old+expr+')';assert src.count(target)==1
 src=src.replace(target,'w->write(w->context,'+kind+','+expr+')')
assert src.count(old+'0x')==8
src=src.replace(old+'0x', 'w->write(w->context,CS_ENTRY,0x')
pathlib.Path(sys.argv[2]).write_text(src)
