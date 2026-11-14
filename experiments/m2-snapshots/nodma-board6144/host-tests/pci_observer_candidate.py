"""Exact isolated observer transform; no in-place edits or VM launch.
Usage: script INPUT EXPECTED_SHA OUTPUT
"""
import hashlib,pathlib,sys
src=pathlib.Path(sys.argv[1]);data=src.read_bytes();assert hashlib.sha256(data).hexdigest()==sys.argv[2]
s=data.decode();out=pathlib.Path(sys.argv[3]);assert not out.exists()
def once(a,b):
 global s
 assert s.count(a)==1,(a,s.count(a));s=s.replace(a,b)
once('#include "firmware_pci_probe.h"','#include "firmware_pci_probe.h"\n#include "pci_probe_nodma.h"')
once('static struct fpp_state pci;','static struct vfb enumeration;\nstatic unsigned enumeration_cycles;')
once('if(!fpp_init(&pci,active_apic.epoch,active_apic.reference,clock_source.max_gap))return false;','if(!vfb_init_nodma_probe(&enumeration,active_apic.epoch,active_apic.reference,clock_source.max_gap))return false;')
once('uint64_t before=native.gpr[0];bool accepted=fpp_io(&pci,','if(enumeration_cycles++>=4096){vfb_abort(&enumeration);return false;}\n  uint64_t before=native.gpr[0];bool accepted=vfb_io(&enumeration,')
s=s.replace('pci.pci.address','enumeration.board.pci.address').replace('pci.pm','enumeration.board.pm')
assert 'fpp_io(' not in s and 'fpp_init(' not in s and '&pci' not in s
s=s.replace('/* fpp committed RIP exactly once. */','/* vfb committed RIP once; failed completion is never replayed. */')
out.write_text(s)
print(hashlib.sha256(s.encode()).hexdigest())
