from pathlib import Path
p=Path('/workspace/shared/code-rom-diag');b=Path('/workspace/shared/ovmf-spike/pre-rom-diag/firmware_vars_rom_test.c').read_text();b=b[:b.index('struct writes')].replace('#include "../firmware_vars_rom.h"','#include "firmware_mmio_memory.h"').replace('#include "../guest_mmio_mov.h"','')
b+='''int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);code[16]=0x78;
 struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+510*8,0x6023);put(m,0x6000+132*8,0xffc84063);
 struct fmm_code_diagnostic d;
 unsigned char*before=malloc((size_t)COUNT*4096);assert(before);memcpy(before,h->bytes,(size_t)COUNT*4096);
 assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d));
 assert(d.code_backing_ok&&d.byte_ok&&d.original==0x78&&d.walk.read_ok&&d.walk.write_ok&&d.walk.read_gpa==0xffc84010&&d.walk.write_gpa==0xffc84010&&d.walk.write_attributes==0x400&&!d.walk.vars_backing_ok);
 assert(d.walk.count==4&&d.walk.ept_count==4&&d.walk.ept_permissions[3]==0x35);
 assert(!memcmp(before,h->bytes,(size_t)COUNT*4096));
 /* Guest D absence is diagnosed, never repaired. */
 put(m,0x6000+132*8,0xffc84023);assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.walk.write_ok&&d.walk.read_ok&&d.code_backing_ok);put(m,0x6000+132*8,0xffc84063);
 put(m,0x6000+132*8,0xffc84061);assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.walk.write_ok);put(m,0x6000+132*8,0xffc84063);
 /* These diagnostics must not be confused with an execution gate. */
 assert(!fmm_validate_vars_write(&f,0xffc84010,1,48,0x5aa,0xffc84010,0xffc84010,0));
 assert(!fmm_validate(&f,0xffc84010,1,true,48,0x5aa,0xffc84010,0xffc84010,0,0xffc84000,4096));
 uint64_t*leaf=(uint64_t*)(h->bytes+(m->tables[68]-BASE));uint64_t old=leaf[132];
 leaf[132]|=2;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok&&!d.byte_ok);leaf[132]=old;
 old=m->ram[8];m->ram[8]=m->flash[132];assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok);m->ram[8]=old;
 old=m->flash[133];m->flash[133]=m->flash[132];assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok);m->flash[133]=old;
 assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc00010,&d)&&!d.code_backing_ok);
 f.cr3=0xffc84000;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.walk.read_ok&&!d.walk.count);f.cr3=0x1000;
 m->layout.code.write=true;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok);m->layout.code.write=false;
 for(unsigned i=1;i<=12;i++){h->fail_map=h->maps+i;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d));h->fail_map=0;}
 assert(fmm_diagnose_code_write(0,0,0,&d)&&!d.code_backing_ok&&!d.walk.mode_ok);
 assert(!memcmp(before,h->bytes,(size_t)COUNT*4096));
 assert(fwm_release(m));free(before);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("CODE diagnostic: exact RX ownership, read/write walk, absent D/W, alias/leaf rejection, immutable bytes PASS");
}
''';(p/'test.c').write_text(b)
