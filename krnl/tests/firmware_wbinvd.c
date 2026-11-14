#include "../firmware_wbinvd.h"
#include <assert.h>
#include <stdio.h>
struct mock {unsigned maintain,write,order,samples,fail_sample;bool fail_maintain,fail_write;uint64_t rip;};
static bool sample(void*x){struct mock*m=x;assert(m->order==0||m->order==2);m->order++;m->samples++;return m->samples!=m->fail_sample;}
static bool maintain(void*x){struct mock*m=x;assert(m->order==1);m->order=2;m->maintain++;return !m->fail_maintain;}
static bool write_rip(void*x,uint32_t field,uint64_t value){struct mock*m=x;assert(m->order==3&&field==0x681e);m->order=4;m->write++;m->rip=value;return !m->fail_write;}
int main(void){
 const uint8_t good[]={0x0f,0x09};struct fwb_state s={0};struct mock m={0};
 assert(fwb_apply(&s,54,0,0x8000,2,0,good,2,true,true,&m,sample,maintain,write_rip));
 assert(!s.failed&&m.maintain==1&&m.write==1&&m.order==4&&m.samples==2&&m.rip==0x8002);
 m=(struct mock){0};assert(!fwb_apply(&s,0,1,UINT64_MAX,0,3,0,0,false,false,&m,sample,maintain,write_rip)&&!s.failed&&!m.order);
 for(unsigned t=0;t<20;t++){
  s=(struct fwb_state){0};m=(struct mock){0};uint32_t reason=54;uint64_t q=0,rip=0x8000;unsigned len=2,cpl=0;size_t n=2;bool admitted=true;uint8_t bytes[]={0x0f,0x09,0x09};
  if(t==0)reason=53;
  if(t==1)q=1; /* WBNOINVD qualification cannot masquerade as WBINVD. */
  if(t==2)len=3;
  if(t==3)cpl=3;
  if(t==4)n=1;
  if(t==5)bytes[0]=0x66; /* prefix */
  if(t==6)bytes[1]=0x08; /* INVD */
  if(t==7)rip=UINT64_MAX;
  if(t==8)rip=0x800000000000ULL;
  if(t==9)rip=0x7fffffffffffULL;
  if(t==10)admitted=false;
  if(t==11)m.fail_maintain=true;
  if(t==12)m.fail_write=true;
  if(t==16)q=UINT64_MAX;
  if(t==17)m.fail_sample=1;
  if(t==18)m.fail_sample=2;
  assert(!fwb_apply(&s,reason,q,rip,len,cpl,t==13?0:bytes,n,true,admitted,&m,t==19?0:sample,t==14?0:maintain,t==15?0:write_rip)&&s.failed);
  assert(m.maintain==((t==11||t==12||t==18)?1u:0u)&&m.write==(t==12?1u:0u));
  assert(m.samples==((t==12||t==18)?2u:(t==11||t==17)?1u:0u));
  unsigned previous=m.order;
  assert(!fwb_apply(&s,54,0,0x8000,2,0,good,2,true,true,&m,sample,maintain,write_rip)&&m.order==previous);
 }
 m=(struct mock){0};assert(!fwb_apply(0,54,0,0x8000,2,0,good,2,true,true,&m,sample,maintain,write_rip)&&!m.order);
 s=(struct fwb_state){0};assert(fwb_apply(&s,54,0,0xffff800000001000ULL,2,0,good,2,true,true,&m,sample,maintain,write_rip)&&m.rip==0xffff800000001002ULL);
 puts("WBINVD host-only transaction: strict admission, callback order, irreversible-failure latch PASS");return 0;
}
