#include "rw_backend.h"
#define N 128u
struct desc {uint64_t addr; uint32_t len; uint16_t flags,next;};
static uint64_t le(const uint8_t *p,unsigned n) {uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(i*8);return v;}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));}
static bool overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m){return a<b+m && b<a+n;}
static bool span(struct vrw *s,uint64_t a,size_t n,bool w){return n && a<=UINT64_MAX-n && s->io.admit(s->io.ctx,a,n,w);}
static int fail(struct vrw *s){s->failed=true;s->busy=false;return -1;}
bool vrw_init(struct vrw*s,struct vro_io io,struct vrw_store disk,uint32_t pfn){
 if(!s)return false;
 *s=(struct vrw){.io=io,.store=disk,.base=(uint64_t)pfn<<12,.failed=true};
 if(!io.admit||!io.read||!io.write||!io.release||!disk.sectors||disk.sectors>UINT64_MAX/512||!disk.read||!disk.write||!disk.flush||!pfn)return false;
 if(!span(s,s->base,8192,false)||!span(s,s->base,8192,true))return false;
 s->failed=false;return true;
}
int vrw_poll(struct vrw *s){
 uint8_t b[16],h[16],seen[N]={0},status=0,element[8],scratch[512];
 struct desc d[N];unsigned count=0,head,cur;uint64_t total=0,data=0,sector,remaining=0;bool writable=false;
 uint32_t kind,used=0;
 if(!s||s->failed)return -1;
 if(s->busy)return fail(s);
 s->busy=true;
 if(!s->io.read(s->io.ctx,s->base+16*N,b,4))return fail(s);
 /* Only polling bit supported; reject unknown avail flags. */
 if(le(b,2)&~1u)return fail(s);
 bool notify=!(le(b,2)&1);
 uint16_t pending=(uint16_t)((uint16_t)le(b+2,2)-s->index);
 if(!pending){s->busy=false;return 0;}
 /* Explicit pilot restriction: no batch ownership claims. */
 if(pending>N)return fail(s);
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
 if(kind==0||kind==1){
  if(!data||data%512||sector>s->store.sectors||data/512>s->store.sectors-sector)status=1;
  else remaining=data;
 }else if(kind==8){if(data<20)status=1;else remaining=20;}
 else if(kind==4){if(data)status=1;}
 else status=2;
 static const uint8_t id[20]={'S','Y','L','E','-','P','E','R','S','I','S','T','E','N','T','-','0','0','0','1'};
 bool prefix=true;unsigned cursor=0;uint64_t position=0;
 for(unsigned i=1;i+1<count;i++){
  uint32_t n=remaining<d[i].len?(uint32_t)remaining:d[i].len;
  if(kind==8&&n){if(!s->io.write(s->io.ctx,d[i].addr,id+(20-remaining),n))return fail(s);}
  else for(uint32_t done=0;done<n;){
   if(!cursor&&kind==0){if(!s->store.read(s->store.ctx,sector+position/512,scratch))return fail(s);s->reads++;}
   unsigned chunk=512-cursor;if(chunk>n-done)chunk=n-done;
   if(kind==0){if(!s->io.write(s->io.ctx,d[i].addr+done,scratch+cursor,chunk))return fail(s);}
   else if(!s->io.read(s->io.ctx,d[i].addr+done,scratch+cursor,chunk))return fail(s);
   cursor+=chunk;done+=chunk;position+=chunk;
   if(cursor==512){
    if(kind==1){if(!s->store.write(s->store.ctx,sector+position/512-1,scratch))return fail(s);s->writes++;}
    cursor=0;
   }
  }
  remaining-=n;
  if(d[i].flags&2){if(prefix)used+=n;if(n<d[i].len)prefix=false;}
 }
 if(!status&&(kind==1||kind==4)){
  if(!s->store.flush(s->store.ctx))return fail(s);
  s->flushes++;
 }
 if(!s->io.write(s->io.ctx,d[count-1].addr,&status,1))return fail(s);
 if(prefix)used++;
 put(element,head,4);put(element+4,used,4);
 if(!s->io.write(s->io.ctx,s->base+4096+4+8*(s->index%N),element,8))return fail(s);
 s->io.release(s->io.ctx);
 put(b,(uint16_t)(s->index+1),2);
 if(!s->io.write(s->io.ctx,s->base+4096+2,b,2))return fail(s);
 s->index++;s->completions++;if(notify)s->interrupt=true;s->busy=false;return 1;
}
