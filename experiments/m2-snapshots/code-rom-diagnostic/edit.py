from pathlib import Path
p=Path('/workspace/shared/code-rom-diag');c=p/'firmware_mmio_memory.c';s=c.read_text();a=s.index('static void diagnose_backing(');z=s.index('\nbool fmm_diagnose_vars_write',a);fn=s[a:z].replace('diagnose_backing(','diagnose_code_backing(').replace('f->layout.vars.kind!=FWL_FLASH_VARS||!f->layout.vars.read||f->layout.vars.write||f->layout.vars.execute','f->layout.code.kind!=FWL_ROM_CODE||!f->layout.code.read||f->layout.code.write||!f->layout.code.execute').replace('!fwl_contains(&f->layout.vars,gpa,1)','!fwl_contains(&f->layout.code,gpa,1)').replace('(token|0x31)','(token|0x35)')
s+='\n/* Read-only CODE diagnostic; never an execution admission gate. */\n'+fn+'''
bool fmm_diagnose_code_write(const struct fmm_memory*m,uint64_t va,uint64_t gpa,
 struct fmm_code_diagnostic*out){
 if(!out)return false;
 struct fmm_code_diagnostic d={0};
 if(!fmm_diagnose_vars_write(m,va,gpa,&d.walk))return false;
 /* The legacy raw-walk helper's VARS result is not a CODE claim. */
 d.walk.vars_backing_ok=false;
 d.walk.ept_count=0;
 for(unsigned i=0;i<4;i++){d.walk.ept_permissions[i]=0;d.walk.ept_matches[i]=false;}
 if(d.walk.mode_ok&&d.walk.canonical){
  diagnose_code_backing(m->memory,gpa,&d.walk);
  d.code_backing_ok=d.walk.backing_stop==FMM_VARS_VALID;
  if(d.code_backing_ok)d.byte_ok=instruction_byte(m,gpa,&d.original);
 }
 *out=d;return true;
}
''';c.write_text(s)
h=p/'firmware_mmio_memory.h';s=h.read_text();s=s.replace('#endif','''/* Read-only CODE RX ownership/EPT diagnosis. walk.vars_backing_ok remains
 * false; no admission/write/commit API is provided. Outputs contain no HPA. */
struct fmm_code_diagnostic {struct fmm_vars_diagnostic walk;bool code_backing_ok,byte_ok;uint8_t original;};
bool fmm_diagnose_code_write(const struct fmm_memory*,uint64_t linear,uint64_t expected_gpa,
 struct fmm_code_diagnostic*);
#endif''');h.write_text(s)
pth=p/'firmware_ovmf_quiet_observe.c';s=pth.read_text();anchor='  if(fwl_contains(&mem.layout.vars,e.physical,1)){' ;assert s.count(anchor)==1
new='''  /* Diagnostic stop only: do not complete/ignore the CODE write or change
   * EPT, guest page tables, flags, RIP, GPR, or any virtual device state. */
  if(e.physical==UINT64_C(0xffc84010)){
   struct fmm_code_diagnostic d;
   io_instruction_diag(rip);
   log_text("OVMF.CODE_DIAG gla=");log_hex(e.linear);log_text(" gpa=");log_hex(e.physical);
   log_text(" rax=");log_hex(native.gpr[0]);log_text(" ds=");log_hex(e.ds_base);
   log_text(" flags=");log_hex(e.rflags);log_text(" vectoring=");log_hex(e.vectoring);log_text("\\n");
   for(unsigned i=0;i<15;i++){log_text("OVMF.CODE_DIAG gpr_index=");log_hex(i);log_text(" value=");log_hex(native.gpr[i]);log_text("\\n");}
   if(fmm_diagnose_code_write(&m,e.linear,e.physical,&d)){
    log_text("OVMF.CODE_DIAG mode=");log_hex(d.walk.mode_ok);log_text(" canonical=");log_hex(d.walk.canonical);
    log_text(" read_ok=");log_hex(d.walk.read_ok);log_text(" read_gpa=");log_hex(d.walk.read_gpa);
    log_text(" read_attrs=");log_hex(d.walk.read_attributes);log_text(" write_ok=");log_hex(d.walk.write_ok);
    log_text(" write_gpa=");log_hex(d.walk.write_gpa);log_text(" write_attrs=");log_hex(d.walk.write_attributes);
    log_text(" code_owner=");log_hex(d.code_backing_ok);log_text(" byte_ok=");log_hex(d.byte_ok);
    log_text(" original=");log_hex(d.original);log_text(" backing_stop=");log_hex(d.walk.backing_stop);
    log_text(" walk_stop=");log_hex(d.walk.stop);log_text("\\n");
    for(unsigned i=0;i<d.walk.ept_count;i++){log_text("OVMF.CODE_DIAG ept_level=");log_hex(i);log_text(" permissions=");log_hex(d.walk.ept_permissions[i]);log_text(" match=");log_hex(d.walk.ept_matches[i]);log_text("\\n");}
    for(unsigned i=0;i<d.walk.count;i++){log_text("OVMF.CODE_DIAG pt_level=");log_hex(i);log_text(" table_gpa=");log_hex(d.walk.entries[i].table_gpa);log_text(" entry_gpa=");log_hex(d.walk.entries[i].entry_gpa);log_text(" value=");log_hex(d.walk.entries[i].value);log_text("\\n");}
   }
   log_text("OVMF.CODE_DIAG stop_only_no_store_no_commit\\n");
   return false;
  }
'''
s=s.replace(anchor,new+anchor);pth.write_text(s)
