#include "backend.h"
#define N 128u
struct desc {uint64_t addr; uint32_t len; uint16_t flags,next;};
static uint64_t le(const uint8_t *p,unsigned n) {uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(i*8);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));}
static bool overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m){return a<b+m && b<a+n;}
static bool span(struct vro *s,uint64_t a,size_t n,bool w){return n && a<=UINT64_MAX-n && s->io.admit(s->io.ctx,a,n,w);}
static int fail(struct vro *s){s->failed=true;s->busy=false;return -1;}
bool vro_init(struct vro *s,struct vro_io io,const uint8_t *disk,size_t size,uint32_t pfn){
 if(!s)return false;
 *s=(struct vro){.io=io,.disk=disk,.disk_size=size,.base=(uint64_t)pfn<<12,.failed=true};
 if(!io.admit||!io.read||!io.write||!io.release||!disk||!size||size%512||!pfn)return false;
 if(!span(s,s->base,8192,false)||!span(s,s->base,8192,true))return false;
 s->failed=false;return true;
}
int vro_poll(struct vro *s){
 uint8_t b[16],h[16],seen[N]={0},status=0,element[8];
 struct desc d[N];unsigned count=0,head,cur;uint64_t total=0,data=0,sector,offset=0,remaining=0;bool writable=false;
 uint32_t kind,used=0;
 if(!s||s->failed)return -1;
 if(s->busy)return fail(s);
 s->busy=true;
 if(!s->io.read(s->io.ctx,s->base+16*N,b,4))return fail(s);
 /* Only polling bit supported; reject unknown avail flags. */
 if(le(b,2)!=1)return fail(s);
 uint16_t pending=(uint16_t)((uint16_t)le(b+2,2)-s->index);
 if(!pending){s->busy=false;return 0;}
 /* Explicit pilot restriction: no batch ownership claims. */
 if(pending!=1)return fail(s);
 if(!s->io.read(s->io.ctx,s->base+16*N+4+2*(s->index%N),b,2))return fail(s);
 head=cur=(unsigned)le(b,2);
 do {
  if(cur>=N||seen[cur]||count>=N)return fail(s);
  seen[cur]=1;
  if(!s->io.read(s->io.ctx,s->base+16*cur,b,16))return fail(s);
  struct desc x={le(b,8),(uint32_t)le(b+8,4),(uint16_t)le(b+12,2),(uint16_t)le(b+14,2)};
  if(x.flags&~3u || !span(s,x.addr,x.len,(x.flags&2)!=0))return fail(s);
  if(overlap(x.addr,x.len,s->base,8192))return fail(s);
  if(writable && !(x.flags&2))return fail(s);
  writable|=(x.flags&2)!=0;
  total+=x.len;if(total>UINT64_C(0x100000000))return fail(s);
  for(unsigned i=0;i<count;i++)if(((x.flags|d[i].flags)&2) && overlap(x.addr,x.len,d[i].addr,d[i].len))return fail(s);
  d[count++]=x;cur=x.next;
 }while(d[count-1].flags&1);
 if(count<2||d[0].len!=16||(d[0].flags&2)||d[count-1].len!=1||!(d[count-1].flags&2))return fail(s);
 if(!s->io.read(s->io.ctx,d[0].addr,h,16))return fail(s);
 kind=(uint32_t)le(h,4);sector=le(h+8,8);
 for(unsigned i=1;i+1<count;i++){
  data+=d[i].len;
  if((kind==0||kind==8)&&!(d[i].flags&2))return fail(s);
  if(kind==1&&(d[i].flags&2))return fail(s);
 }
 if(data>UINT32_MAX-1)return fail(s);
 if(kind==0){
  if(!data||data%512||sector>s->disk_size/512||data/512>s->disk_size/512-sector)status=1;
  else {remaining=data;offset=sector*512;}
 }else if(kind==8){if(data<20)status=1;else remaining=20;}
 else status=kind==1?1:2;
 /* Snapshot/admission complete before first mutation. Failed stores are terminal,
  * including used.idx failures that might already have published completion. */
 static const uint8_t id[20]={'S','Y','L','E','-','R','E','A','D','O','N','L','Y','-','D','I','S','K','-','1'};
 bool prefix=true;
 for(unsigned i=1;i+1<count;i++){
  uint32_t n=remaining<d[i].len?(uint32_t)remaining:d[i].len;
  if(n){const uint8_t *src=kind==8?id+(20-remaining):s->disk+offset;
   if(!s->io.write(s->io.ctx,d[i].addr,src,n))return fail(s);
   remaining-=n;offset+=n;
  }
  if(d[i].flags&2){if(prefix)used+=n;if(n<d[i].len)prefix=false;}
 }
 if(!s->io.write(s->io.ctx,d[count-1].addr,&status,1))return fail(s);
 if(prefix)used++;
 put(element,head,4);put(element+4,used,4);
 if(!s->io.write(s->io.ctx,s->base+4096+4+8*(s->index%N),element,8))return fail(s);
 s->io.release(s->io.ctx);
 put(b,(uint16_t)(s->index+1),2);
 if(!s->io.write(s->io.ctx,s->base+4096+2,b,2))return fail(s);
 s->index++;s->busy=false;return 1;
}
