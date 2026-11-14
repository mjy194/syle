#include "platform.h"
static bool overlaps(uint64_t a,uint64_t n,uint64_t b,uint64_t m){return a<b+m&&b<a+n;}
static bool valid(uint64_t a,uint64_t n){return n&&a<=UINT64_MAX-n;}
static struct vrp_region *find(struct vrp *s,uint64_t a,size_t n,bool w){
 if(!s->active||!valid(a,n))return 0;
 for(unsigned i=0;i<s->count;i++){struct vrp_region *r=&s->regions[i];
  if(a>=r->gpa && a-r->gpa<=r->size && n<=r->size-(size_t)(a-r->gpa) && (w?r->write:r->read))return r;
 }
 return 0;
}
static bool admit(void *c,uint64_t a,size_t n,bool w){return find(c,a,n,w)!=0;}
static bool readmem(void *c,uint64_t a,void *b,size_t n){struct vrp_region *r=find(c,a,n,false);if(!r)return false;uint8_t *d=b;for(size_t i=0;i<n;i++)d[i]=r->host[a-r->gpa+i];return true;}
static bool writemem(void *c,uint64_t a,const void *b,size_t n){struct vrp_region *r=find(c,a,n,true);if(!r)return false;const uint8_t *d=b;for(size_t i=0;i<n;i++)r->host[a-r->gpa+i]=d[i];return true;}
static void barrier(void *c){((struct vrp*)c)->barriers++;}
void vrp_abort(struct vrp *s){if(s){s->active=false;s->dev.failed=true;s->dev.backend.failed=true;}}
bool vrp_init(struct vrp *s,uint16_t bdf,uint64_t owner,const struct vrp_region *regions,unsigned count,const struct vrp_ports *ports,unsigned pn,const uint8_t *disk,size_t size){
 if(!s)return false;
 *s=(struct vrp){.bdf=bdf};
 if(!owner||!regions||!count||count>VRP_REGIONS||pn>VRP_RESERVED||(pn&&!ports)||!disk||!valid((uintptr_t)disk,size))return false;
 for(unsigned i=0;i<count;i++){
  struct vrp_region r=regions[i];
  if(r.owner!=owner||!r.ram||!r.host||!valid(r.gpa,r.size)||!valid((uintptr_t)r.host,r.size)||overlaps((uintptr_t)r.host,r.size,(uintptr_t)disk,size)||overlaps((uintptr_t)r.host,r.size,(uintptr_t)s,sizeof(*s)))return false;
  for(unsigned j=0;j<i;j++)if(overlaps(r.gpa,r.size,s->regions[j].gpa,s->regions[j].size)||overlaps((uintptr_t)r.host,r.size,(uintptr_t)s->regions[j].host,s->regions[j].size))return false;
  s->regions[i]=r;
 }
 for(unsigned i=0;i<pn;i++){if(ports[i].end>65536||ports[i].end<=ports[i].first)return false;s->reserved[i]=ports[i];}
 s->count=count;s->reserved_count=pn;s->active=true;
 struct vro_io io={s,admit,readmem,writemem,barrier};
 if(!vrt_init(&s->dev,io,disk,size)){vrp_abort(s);return false;}
 return true;
}
int vrp_config(struct vrp *s,uint16_t bdf,unsigned off,unsigned n,bool write,uint32_t *v){
 if(!s||!s->active||s->dev.failed)return -1;
 if(bdf!=s->bdf)return 0;
 if(v&&write&&off==16&&n==4&&*v!=UINT32_MAX){uint32_t base=*v&0xffffffc0u;
  if(base)for(unsigned i=0;i<s->reserved_count;i++)if(overlaps(base,64,s->reserved[i].first,s->reserved[i].end-s->reserved[i].first)){vrp_abort(s);return -1;}
 }
 if(!vrt_config(&s->dev,off,n,write,v)){vrp_abort(s);return -1;}
 return 1;
}
int vrp_io(struct vrp *s,uint16_t port,unsigned n,bool write,uint32_t *v){
 if(!s||!s->active||s->dev.failed)return -1;
 if(!s->dev.bar||s->dev.probe||!(s->dev.command&1)||port<s->dev.bar||(uint32_t)port-s->dev.bar>=64)return 0;
 /* Strict reset epoch: the caller must supply a fresh queue. Detect stale
  * publication rather than resetting/wiping RAM owned by the guest. */
 if(v&&write&&port-s->dev.bar==8&&n==4&&*v){
  uint64_t base=(uint64_t)*v<<12;uint8_t a[2],u[2];
  if(!readmem(s,base+16*128+2,a,2)||!readmem(s,base+4096+2,u,2)||a[0]||a[1]||u[0]||u[1]){vrp_abort(s);return -1;}
 }
 if(!vrt_io(&s->dev,port,n,write,v)){vrp_abort(s);return -1;}
 return 1;
}
