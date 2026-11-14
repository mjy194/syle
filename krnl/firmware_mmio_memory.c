#include "firmware_mmio_memory.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
/* Only separately admitted IRQ DE profile; default binary mask unchanged. */
#ifdef SYLE_FMM_DE_CONFIG
#define FMM_CR4_ALLOWED UINT64_C(0x2668)
#else
#define FMM_CR4_ALLOWED UINT64_C(0x2660)
#endif
static bool mode(const struct fmm_memory*m){return m&&m->memory&&m->memory->prepared&&!m->memory->quarantined&&m->memory->owner.map&&
 (m->cr0&0x80000031)==0x80000031&&!(m->cr0&~UINT64_C(0x8005003b))&&
 (m->cr4&32)&&!(m->cr4&~FMM_CR4_ALLOWED)&&
 m->efer==0x500&&
 !(m->cr3&~UINT64_C(0xfffff018))&&!m->cs_base&&m->cs_selector&&!(m->cs_selector&7)&&
 (m->cs_ar&~1u)==0xa09a;}
static bool read64(const struct fmm_memory*m,uint64_t p,uint64_t*out){
 if(p>FWM_RAM_SIZE-8)return false;
 if(!fwm_owned_page(m->memory,m->memory->ram[p/4096]))return false;
 uint8_t b[8];if(!fwm_read_ram(m->memory,p,b,8))return false;
 uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)b[i]<<(i*8);*out=v;return true;
}
static bool translate(const struct fmm_memory*m,uint64_t va,enum fmm_access access,uint64_t*out,uint64_t *attributes){
 if(!out||!mode(m)||!canonical(va)||access>FMM_EXEC||(int)access<0)return false;
 uint64_t table=m->cr3&UINT64_C(0xfffff000);bool rw=true,user=true;
 for(unsigned level=0;level<4;level++){
  if(table>FWM_RAM_SIZE-4096)return false;
  unsigned shift=39-9*level;uint64_t e;
  if(!read64(m,table+((va>>shift)&511)*8,&e)||!(e&1)||!(e&32)||
     (e&UINT64_C(0xffffffff00000000)))return false;
  rw=rw&&!!(e&2);user=user&&!!(e&4);
  if(access==FMM_WRITE&&!rw&&(m->cr0&(1u<<16)))return false;
  bool huge=(e&128)&&level==2;
  if((e&128)&&level<2)return false;
  if(level==3||huge){
   if(access==FMM_WRITE&&!(e&64))return false;
   uint64_t mask=huge?UINT64_C(0x1fffff):4095;
   if(huge&&(e&UINT64_C(0x1fe000)))return false;
   *out=(e&UINT64_C(0xfffff000)&~mask)|(va&mask);if(attributes)*attributes=(rw?0x400:0)|(user?0x200:0);return true;
  }
  table=e&UINT64_C(0xfffff000);
 }
 return false;
}
bool fmm_translate(const struct fmm_memory*m,uint64_t va,enum fmm_access access,uint64_t*out){
 return translate(m,va,access,out,0);
}
/* Only translate trusted region metadata to an owned token, never cast a GPA.
 * Caller freezes memory ownership and page tables while this operation runs. */
static bool instruction_byte(const struct fmm_memory*m,uint64_t p,uint8_t*out){
 struct firmware_memory*f=m->memory;
 if(p<FWM_RAM_SIZE){
  return fwm_owned_page(f,f->ram[p/4096])&&fwm_read_ram(f,p,out,1);
 }
 const struct fwl_region*c=&f->layout.code;
 if(c->kind!=FWL_ROM_CODE||!c->read||!c->execute||c->write||
    !fwl_contains(c,p,1)||c->base<f->layout.vars.base)return false;
 uint64_t i=(p-f->layout.vars.base)/4096;
 if(i>=f->flash_pages||i>=FWM_FLASH_PAGES||!fwm_owned_page(f,f->flash[i]))return false;
 const uint8_t*v=f->owner.map(f->owner.context,f->flash[i]);
 if(!v||((uintptr_t)v&4095))return false;
 *out=v[p&4095];return true;
}
size_t fmm_fetch(const struct fmm_memory*m,uint64_t rip,uint8_t b[15]){
 if(!b)return 0;
 size_t n=0;for(;n<15;n++){
  if(rip>UINT64_MAX-n)break;
  uint64_t p;if(!fmm_translate(m,rip+n,FMM_EXEC,&p)||!instruction_byte(m,p,&b[n]))break;
 }
 return n;
}
bool fmm_validate(const struct fmm_memory*m,uint64_t va,unsigned width,bool write,
 uint32_t reason,uint64_t q,uint64_t gla,uint64_t gpa,uint32_t vector,uint64_t base,uint64_t size){
 if(!mode(m)||base<FWM_RAM_SIZE||base>=UINT64_C(0x100000000)||size>UINT64_C(0x100000000)-base||
    base>=m->memory->layout.vars.base||size>m->memory->layout.vars.base-base||
    reason!=48||vector||(q&~UINT64_C(0x400))!=(write?UINT64_C(0x182):UINT64_C(0x181))||va!=gla||
    (width!=1&&width!=2&&width!=4&&width!=8)||!size||base>UINT64_MAX-(size-1)||
    gpa<base||gpa-base>=size||size-(gpa-base)<width||va>UINT64_MAX-(width-1)||
    (va&4095)+width>4096||gpa>UINT64_MAX-(width-1))return false;
 for(unsigned i=0;i<width;i++){
  uint64_t p,attributes=0;if(!translate(m,va+i,write?FMM_WRITE:FMM_READ,&p,&attributes)||p!=gpa+i)return false;
  if(m->advanced_ept_info?((q&0x600)!=attributes):!!(q&0x400))return false;
 }
 return true;
}

/* Separate immutable VARS path: never relax the device-hole validator. */
static bool vars_token(const struct firmware_memory*f,uint64_t gpa,uint64_t*out){
 if(!f||!f->prepared||f->quarantined||!f->owner.map||!out||
 f->layout.ram.base||f->layout.ram.size!=FWM_RAM_SIZE||
 f->layout.vars.base!=UINT64_C(0xffc00000)||f->layout.vars.size!=540672||
 f->layout.code.base!=UINT64_C(0xffc84000)||f->layout.code.size!=3653632||
 f->flash_pages!=1024||f->layout.vars.kind!=FWL_FLASH_VARS||!f->layout.vars.read||f->layout.vars.write||f->layout.vars.execute||
 !fwl_contains(&f->layout.vars,gpa,1)||gpa<UINT64_C(0xffc00000)||gpa>=FWL_TOP||
 (f->layout.vars.base&4095)||f->flash_pages>FWM_FLASH_PAGES||f->eptp!=(f->tables[0]|0x1e))return false;
 uint64_t index=(gpa-f->layout.vars.base)/4096;if(index>=f->flash_pages)return false;
 uint64_t token=f->flash[index];if(!fwm_owned_page(f,token))return false;
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(f->ram[i]==token)return false;
 for(unsigned i=0;i<FWM_FLASH_PAGES;i++)if(i!=index&&f->flash[i]==token)return false;
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(f->tables[i]==token)return false;
 /* This builder uses a fixed four-level topology for the last4MiB. Verify
  * actual links/permissions, not merely the intended region metadata. */
 unsigned slots[]={0,1,3,68+(unsigned)((gpa-UINT64_C(0xffc00000))>>21)};
 unsigned shifts[]={39,30,21,12};
 for(unsigned level=0;level<4;level++){
  uint64_t pa=f->tables[slots[level]];if(!fwm_owned_page(f,pa))return false;
  for(unsigned earlier=0;earlier<level;earlier++)if(pa==f->tables[slots[earlier]])return false;
  const uint8_t*p=f->owner.map(f->owner.context,pa);if(!p||((uintptr_t)p&4095))return false;
  unsigned off=(unsigned)((gpa>>shifts[level])&511)*8;uint64_t entry=0;
  for(unsigned i=0;i<8;i++)entry|=(uint64_t)p[off+i]<<(i*8);
  uint64_t expected=level==3?(token|0x31):(f->tables[slots[level+1]]|7);
  if(entry!=expected)return false;
 }
 *out=token;return true;
}
bool fmm_read_vars_byte(const struct firmware_memory*f,uint64_t gpa,uint8_t*out){
 uint64_t token;if(!out||!vars_token(f,gpa,&token))return false;
 const uint8_t*p=f->owner.map(f->owner.context,token);if(!p||((uintptr_t)p&4095))return false;
 *out=p[gpa&4095];return true;
}
bool fmm_validate_vars_write(const struct fmm_memory*m,uint64_t va,unsigned width,
 uint32_t reason,uint64_t q,uint64_t gla,uint64_t gpa,uint32_t vector){
 if(!mode(m)||width!=1||reason!=48||vector||va!=gla||(q&~UINT64_C(0x400))!=0x18a)return false;
 uint64_t p,attrs=0,token;
 if(!translate(m,va,FMM_WRITE,&p,&attrs)||p!=gpa||
 (m->advanced_ept_info?((q&0x600)!=attrs):!!(q&0x400)))return false;
 return vars_token(m->memory,gpa,&token);
}

/* Diagnostic duplicate of VARS admission, not an alternate execution gate.
 * Only permission bits/matches are exported, never host physical tokens. */
static void diagnose_backing(const struct firmware_memory*f,uint64_t gpa,struct fmm_vars_diagnostic*d){
 d->backing_stop=FMM_VARS_LAYOUT;
 if(!f||!f->prepared||f->quarantined||!f->owner.map||
 f->layout.ram.base||f->layout.ram.size!=FWM_RAM_SIZE||
 f->layout.vars.base!=UINT64_C(0xffc00000)||f->layout.vars.size!=540672||
 f->layout.code.base!=UINT64_C(0xffc84000)||f->layout.code.size!=3653632||f->flash_pages!=1024||
 f->layout.vars.kind!=FWL_FLASH_VARS||!f->layout.vars.read||f->layout.vars.write||f->layout.vars.execute||
 !fwl_contains(&f->layout.vars,gpa,1)||gpa<UINT64_C(0xffc00000)||gpa>=FWL_TOP||
 (f->layout.vars.base&4095)||f->eptp!=(f->tables[0]|0x1e))return;
 uint64_t index=(gpa-f->layout.vars.base)/4096;if(index>=f->flash_pages)return;
 uint64_t token=f->flash[index];d->backing_stop=FMM_VARS_OWNER;if(!fwm_owned_page(f,token))return;
 d->backing_stop=FMM_VARS_ALIAS;
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(f->ram[i]==token)return;
 for(unsigned i=0;i<FWM_FLASH_PAGES;i++)if(i!=index&&f->flash[i]==token)return;
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(f->tables[i]==token)return;
 unsigned slots[]={0,1,3,68+(unsigned)((gpa-UINT64_C(0xffc00000))>>21)};
 unsigned shifts[]={39,30,21,12};
 for(unsigned level=0;level<4;level++){
  uint64_t pa=f->tables[slots[level]];d->backing_stop=FMM_VARS_OWNER;if(!fwm_owned_page(f,pa))return;
  d->backing_stop=FMM_VARS_ALIAS;
  for(unsigned earlier=0;earlier<level;earlier++)if(pa==f->tables[slots[earlier]])return;
  d->backing_stop=FMM_VARS_EPT_READ;const uint8_t*p=f->owner.map(f->owner.context,pa);
  if(!p||((uintptr_t)p&4095))return;
  unsigned off=(unsigned)((gpa>>shifts[level])&511)*8;uint64_t entry=0;
  for(unsigned i=0;i<8;i++)entry|=(uint64_t)p[off+i]<<(i*8);
  uint64_t expected=level==3?(token|0x31):(f->tables[slots[level+1]]|7);
  d->ept_permissions[level]=(uint8_t)(entry&0xff);d->ept_matches[level]=entry==expected;d->ept_count++;
  d->backing_stop=level==3?FMM_VARS_LEAF:FMM_VARS_PARENT;if(entry!=expected)return;
 }
 d->backing_stop=FMM_VARS_VALID;
}
bool fmm_diagnose_vars_write(const struct fmm_memory*m,uint64_t va,uint64_t expected_gpa,struct fmm_vars_diagnostic*out){
 if(!out)return false;
 struct fmm_vars_diagnostic d={0};
 d.mode_ok=mode(m);d.canonical=canonical(va);
 if(!d.mode_ok||!d.canonical){*out=d;return true;}
 d.read_ok=translate(m,va,FMM_READ,&d.read_gpa,&d.read_attributes);
 d.write_ok=translate(m,va,FMM_WRITE,&d.write_gpa,&d.write_attributes);
 uint64_t token;d.vars_backing_ok=vars_token(m->memory,expected_gpa,&token);
 diagnose_backing(m->memory,expected_gpa,&d);
 uint64_t table=m->cr3&UINT64_C(0xfffff000);
 for(unsigned level=0;level<4;level++){
  if(table>FWM_RAM_SIZE-4096){d.stop=FMM_DIAG_TABLE_RANGE;break;}
  if(!fwm_owned_page(m->memory,m->memory->ram[table/4096])){d.stop=FMM_DIAG_TABLE_OWNER;break;}
  uint64_t address=table+((va>>(39-9*level))&511)*8,e;
  if(!read64(m,address,&e)){d.stop=FMM_DIAG_TABLE_READ;break;}
  d.entries[d.count++]=(struct fmm_walk_entry){table,address,e};
  if(!(e&1)||!(e&32)||(e&UINT64_C(0xffffffff00000000))||((e&128)&&level<2)){
   d.stop=FMM_DIAG_ENTRY_SHAPE;break;
  }
  if(level==3||((e&128)&&level==2)){
   d.stop=((level==2)&&(e&UINT64_C(0x1fe000)))?FMM_DIAG_ENTRY_SHAPE:FMM_DIAG_LEAF;break;
  }
  table=e&UINT64_C(0xfffff000);
 }
 *out=d;return true;
}
