#include "firmware_display.h"
/* Supervisor-only, stopped-vCPU scanout. No guest address is a host pointer.
 * The boot GOP mapping is prevalidated and reserved by PMM/paging. */
static struct syle_framebuffer target;
static struct fdp_stats stats;
static bool bound;
static uint32_t row[1024];
static void clear(void){if(!bound)return;volatile uint32_t*p=(void*)(uintptr_t)target.base;for(uint32_t y=0;y<target.height;y++)for(uint32_t x=0;x<target.width;x++)p[(uint64_t)y*target.stride+x]=0;}
bool firmware_display_bind(const struct syle_framebuffer*f){
 if(bound||!f||f->valid!=1||f->format>1||!f->base||(f->base&3)||f->width<1024||f->height<768||f->width>8192||f->height>8192||f->stride<f->width||f->stride>8192||
 (uint64_t)f->stride*f->height*4>f->size||f->base>UINT64_MAX-f->size)return false;
 target=*f;bound=true;clear();return true;
}
void firmware_display_reset(void){stats=(struct fdp_stats){0};clear();}
struct fdp_stats firmware_display_stats(void){return stats;}
void firmware_display_revoke(void){clear();stats.active=false;}
bool firmware_display_present(struct firmware_memory*m,const struct frfb_state*s,uint64_t reference,bool force){
 if(!bound||!m||!m->prepared||m->quarantined||!s)return false;
 if(!s->active)return true;
 if(!force&&stats.frames&&reference-stats.last_reference<1000000)return true;
 const struct ramfb_config*c=&s->config;
 if(c->width>1024||c->height>768||c->stride<c->width*4||!c->span||c->address>=FWM_RAM_SIZE||c->span>FWM_RAM_SIZE-c->address)return false;
 for(uint64_t p=c->address/4096;p<=(c->address+c->span-1)/4096;p++)if(!fwm_owned_page(m,m->ram[p]))return false;
 uint64_t hash=UINT64_C(1469598103934665603);volatile uint32_t*p=(void*)(uintptr_t)target.base;
 for(uint32_t y=0;y<c->height;y++){
  if(!fwm_read_ram(m,c->address+(uint64_t)y*c->stride,row,c->width*4))return false;
  for(uint32_t x=0;x<c->width;x++){
   uint32_t v=row[x]&0xffffff;hash^=v;hash*=UINT64_C(1099511628211);
   if(target.format==0)v=((v&255)<<16)|(v&0xff00)|((v>>16)&255);
   p[(uint64_t)y*target.stride+x]=v;
  }
 }
 stats.frames++;stats.pixels+=(uint64_t)c->width*c->height;stats.last_hash=hash;stats.last_reference=reference;stats.active=true;return true;
}
