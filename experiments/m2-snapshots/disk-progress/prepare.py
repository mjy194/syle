#!/usr/bin/env python3
"""Fork the accepted serial runtime with one polling read-only block device."""
from pathlib import Path
import argparse,hashlib,json,shutil
from virtualwire import patch as wire_patch, guard as wire_guard
from gp import patch as gp_patch
from user import patch as user_patch, guard as user_guard

def once(s,a,b):
 if s.count(a)!=1:raise ValueError('anchor mismatch: '+a[:100])
 return s.replace(a,b)
def user_memory(s):
 s=once(s,'!(m->cr3&~UINT64_C(0xfffff018))&&!m->cs_base&&m->cs_selector&&!(m->cs_selector&7)&&\n (m->cs_ar&~1u)==0xa09a;','''!(m->cr3&~UINT64_C(0xfffff018))&&!m->cs_base&&m->cs_selector&&
#ifdef SYLE_FIRMWARE_BOOT_LINUX
 (((m->cs_selector&7)==0&&(m->cs_ar&~1u)==0xa09a)||((m->cs_selector&7)==3&&(m->cs_ar&~1u)==0xa0fa));
#else
 !(m->cs_selector&7)&&(m->cs_ar&~1u)==0xa09a;
#endif\n''')
 return once(s,'rw=rw&&!!(e&2);user=user&&!!(e&4);','''rw=rw&&!!(e&2);user=user&&!!(e&4);
#ifdef SYLE_FIRMWARE_BOOT_LINUX
  if((m->cs_selector&3)==3&&!user)return false;
#endif''')
def patch(s):
 s=once(s,'#include "firmware_active_cpu_tsc.h"','''#include "firmware_active_cpu_tsc.h"
#include "firmware_boot_cpu.h"
#define fcats_query fboot_query
#define fcp_efer_allowed fboot_efer_allowed''')
 s=once(s,'efer==0x500&&rd(0x0802','(efer&~UINT64_C(1))==0x500&&rd(0x0802')
 s=once(s,'!(cr4&~UINT64_C(0x2668))','!(cr4&~UINT64_C(0x26f8))')
 s=once(s,'static bool dispatch(uint64_t reason){','#ifdef SYLE_FIRMWARE_DISK\n#include "firmware_disk_observe.inc"\n#endif\nstatic bool dispatch(uint64_t reason){')
 s=once(s,'  if(fcon_port(&console_state,(uint16_t)port))', '''#ifdef SYLE_FIRMWARE_DISK
  if(port==0x61||port==0x42||(port==0x43&&((uint8_t)native.gpr[0]>>6)==2))return disk_pit2_io(rip,q,(unsigned)len);
  if(disk_port((uint16_t)port))return disk_dispatch(rip,q,(unsigned)len,false);
  if(port>=0xcfc&&port<=0xcff&&(pci.pci.address&0x80ffff00)==0x80001800)return disk_dispatch(rip,q,(unsigned)len,true);
#endif
  if(fcon_port(&console_state,(uint16_t)port))''')
 s=once(s,'return debug_read_dispatch(rip,q,(unsigned)len,flags,blocking,pending_debug,(uint32_t)vector);','return disk_debug(rip,q,(unsigned)len,flags,blocking,pending_debug,(uint32_t)vector);')
 s=once(s,' if(reason==16){', '#ifdef SYLE_FIRMWARE_DISK\n if(reason==14)return disk_invlpg(rip,q,(unsigned)len);\n if((reason==31||reason==32)&&disk_linux_msr_supported((uint32_t)native.gpr[2]))return disk_gs_msr(rip,(unsigned)reason,(unsigned)len);\n#endif\n if(reason==16){')
 s=once(s,'  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;','  if(mode.ia32e&&!disk_cr_encoding(rip,q,(unsigned)len))return false;\n  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;')
 s=once(s,'q!=4||len!=3||count<3||bytes[0]!=0x0f||bytes[1]!=0x22||bytes[2]!=0xe0||\n     mode.cr4!=0x660||v!=0x668||','!fboot_cr_encoding(q,(unsigned)len,bytes,count)||')
 s=once(s,'hw!=0x2668||shadow!=0x668','hw!=(v|0x2000)||shadow!=v')
 s=once(s,'ir=fils_write(', 'ir=film_write(')
 s=once(s,' if(!fvr_init(&vars_rom,&mem))', '''#ifdef SYLE_FIRMWARE_DISK
 struct vro_io disk_io;
 if(!disk_bytes||!vfr_bind(&disk_ram,&mem,&disk_io)||!vrt_init(&disk_state,disk_io,disk_bytes,disk_size))goto cleanup;
#endif
 if(!fvr_init(&vars_rom,&mem))''')
 s=once(s,' if(!fwm_release(&mem))fatal();', '''#ifdef SYLE_FIRMWARE_DISK
 vfr_abort(&disk_ram);
#endif
 if(!fwm_release(&mem))fatal();''')
 s=once(s,'  native.resume=1;\n }\n', '''#ifdef SYLE_FIRMWARE_DISK_PROBE
  if(console_matches("SYLE_DISK_FILE_VERIFIED")){
   console_sample_quiet=false;observed++;log_text("OVMF.DISK file_probe_complete\\n");break;
  }
#endif
  native.resume=1;
 }
''')
 s=once(s,' /* Observation only: never report boot success, even after the exit budget. */','''#ifdef SYLE_FIRMWARE_DISK
 log_text("OVMF.DISK terminal bar=");log_hex(disk_state.bar);log_text(" command=");log_hex(disk_state.command);
 log_text(" status=");log_hex(disk_state.status);log_text(" pfn=");log_hex(disk_state.pfn);
 log_text(" notifies=");log_hex(disk_notifies);log_text(" completions=");log_hex(disk_completions);
 log_text(" failed=");log_hex(disk_state.failed);log_text(" backend_failed=");log_hex(disk_state.backend.failed);log_text("\\n");
#endif
 /* Observation only: never report boot success, even after the exit budget. */''')
 return user_guard(user_patch(gp_patch(wire_guard(wire_patch(s,once),once),once),once),once)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--linux',action='store_true');p.add_argument('--linux-console',action='store_true');a=p.parse_args();repo=Path(__file__).resolve().parents[3]
 paths=[]
 for line in (a.base/'source.sha256').read_text().splitlines():
  digest,rel=line.split('  ',1);f=a.base/'source'/rel
  assert not Path(rel).is_absolute() and '..' not in Path(rel).parts and not f.is_symlink() and hashlib.sha256(f.read_bytes()).hexdigest()==digest;paths.append(rel)
 observer=a.base/'source/krnl/firmware_ovmf_quiet_observe.c'
 assert hashlib.sha256(observer.read_bytes()).hexdigest()=='a9fd93f4ba000d5951800119747095cf30d05b30b7aece9fb3555ec861511376'
 a.output.mkdir(parents=True,exist_ok=False);dest=a.output/'source'
 for rel in paths:
  f=dest/rel;f.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(a.base/'source'/rel,f)
 files=['krnl/firmware_boot_debug.c','krnl/firmware_boot_debug.h','krnl/firmware_apic_guard.c','krnl/firmware_apic_guard.h','krnl/firmware_boot_gp.c','krnl/firmware_boot_gp.h','krnl/firmware_disk.h','krnl/firmware_disk_observe.inc','efi/firmware-disk.mk','efi/src/main.c','krnl/main.c','krnl/runtime.c','krnl/firmware_irq_lvt_mask.c','krnl/firmware_irq_lvt_mask.h','krnl/firmware_timer_irq_owner.c','krnl/firmware_timer_irq_owner.h','krnl/firmware_timer_irq_import.c','krnl/firmware_timer_irq_session_import.c','krnl/firmware_irq_apic.c','krnl/firmware_boot_cpu.c','krnl/firmware_boot_cpu.h','krnl/firmware_cpu_mode.c','krnl/firmware_boot_invlpg.c','krnl/firmware_boot_invlpg.h']+[str(f.relative_to(repo)) for f in (repo/'krnl/virtio_blk').iterdir()]
 for rel in files:
  f=dest/rel;f.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/rel,f)
 f=dest/'krnl/firmware_mmio_memory.c';memtext=f.read_text().replace('#ifdef SYLE_FMM_DE_CONFIG','#ifdef SYLE_FIRMWARE_BOOT_LINUX\n#define FMM_CR4_ALLOWED UINT64_C(0x26f8)\n#elif defined(SYLE_FMM_DE_CONFIG)').replace(' m->efer==0x500&&','\n#ifdef SYLE_FIRMWARE_BOOT_LINUX\n (m->efer&~UINT64_C(1))==0x500&&\n#else\n m->efer==0x500&&\n#endif');memtext=user_memory(memtext);f.write_text(memtext)
 q=dest/'efi/firmware-ovmf-quiet.mk' ;q.write_text(q.read_text().replace('$(filter %.o,$^)','$(filter-out $(FQ_LINK_EXCLUDE),$(filter %.o,$^))'))
 q.write_text(q.read_text().replace('$(filter-out $(FQ_LINK_EXCLUDE),$(filter %.o,$^)) -o $@','$(filter-out $(FQ_LINK_EXCLUDE),$(filter %.o,$^)) -o $@\n\t$(if $(FQ_POST_LINK),$(FQ_POST_LINK) $@)'))
 # Reuse the transcript matcher for disk probe, but disable old command stop.
 f=dest/'krnl/firmware_console_observe.inc';f.write_text(f.read_text().replace('#ifdef SYLE_FIRMWARE_CONSOLE_PROBE','#if defined(SYLE_FIRMWARE_CONSOLE_PROBE) || defined(SYLE_FIRMWARE_DISK_PROBE)'))
 text=patch(observer.read_text())
 if a.linux:
  text=text.replace('console_matches("SYLE_DISK_FILE_VERIFIED")','console_matches("SYLE_LINUX_USERSPACE_READY")')
  text=text.replace('console_sample_quiet=false;log_text("OVMF.PM_DELAY dispatch_rejected reason=");','console_sample_quiet=false;io_instruction_diag(rip);log_text("OVMF.DISK rejected_rax=");log_hex(native.gpr[0]);log_text(" rcx=");log_hex(native.gpr[2]);log_text(" rdx=");log_hex(native.gpr[3]);log_text("\\n");log_text("OVMF.PM_DELAY dispatch_rejected reason=");')
 if a.linux_console:
  assert a.linux
  text=text.replace('console_matches("SYLE_LINUX_USERSPACE_READY")', 'console_matches("\\nSYLE_LINUX_DISK_COMMAND_OK_20261118\\r\\n") && console_matches("\\nLinux\\r\\n") && console_matches("\\n6.12.0-syle-efi1\\r\\n") && console_matches("\\n# ")>=4')
 (dest/'krnl/firmware_ovmf_quiet_observe.c').write_text(text)
 (dest/'efi/firmware-disk-runtime.mk').write_text('build/fdisk-observer.o: KFLAGS += -DFPM_DELAY_EXIT_CAP=131072 -DSYLE_FIRMWARE_DISK_PROBE\n')
 (a.output/'source.sha256').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+str(f.relative_to(dest))+'\n' for f in sorted(dest.rglob('*')) if f.is_file()))
 (a.output/'preparation.json').write_text(json.dumps({'base':str(a.base),'prior_wbinvd':50,'disk_bdf':'00:03.0','profile':'read-only legacy block, polled, no disk IRQ'},indent=2))
