#include "firmware_ram_adapter.h"
static bool admit(void*x,uint64_t a,size_t n,bool write){
 struct vfr*s=x;(void)write;
 if(!s||!s->active||!s->memory||!s->memory->prepared||s->memory->quarantined||!n||a>=FWM_RAM_SIZE||n>FWM_RAM_SIZE-a)return false;
 struct firmware_memory*m=s->memory;
 /* Actual low-RAM ledger only. FLASH/MMIO/EPT pages are not valid GPAs here.
  * Membership detects missing records, not global EPT aliases or sharing. */
 uint64_t end=a+n;
 for(uint64_t p=a/4096;p<=(end-1)/4096;p++)if(!fwm_owned_page(m,m->ram[p]))return false;
 return true;
}
static bool read_ram(void*x,uint64_t a,void*b,size_t n){
 struct vfr*s=x;
 if(!admit(s,a,n,false)||!fwm_read_ram(s->memory,a,b,n)){if(s)s->active=false;return false;}return true;
}
static bool write_ram(void*x,uint64_t a,const void*b,size_t n){
 struct vfr*s=x;
 if(!admit(s,a,n,true)||!fwm_write_ram(s->memory,a,b,n)){if(s)s->active=false;return false;}return true;
}
static void publish(void*x){struct vfr*s=x;if(s->active)s->publications++;}
bool vfr_bind(struct vfr*s,struct firmware_memory*m,struct vro_io*out){
 if(!s||!m||!out||!m->prepared||m->quarantined)return false;
 /* Fresh nonalias objects only; no live rebinding. Controller enforces this. */
 *s=(struct vfr){.memory=m,.active=true};
 *out=(struct vro_io){s,admit,read_ram,write_ram,publish};return true;
}
void vfr_abort(struct vfr*s){if(s)s->active=false;}
