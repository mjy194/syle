#include "../linux_msr_bank.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct writes{unsigned n,fail;uint32_t f[3];uint64_t v[3];};
static bool write(void*p,uint32_t f,uint64_t v){struct writes*w=p;assert(w->n<3);w->f[w->n]=f;w->v[w->n]=v;w->n++;return w->n!=w->fail;}
#define B (1u<<5)
#define E ((1u<<29)|(1u<<11))
static bool call(uint64_t*g,struct lvs_state*s,struct writes*w,uint64_t reason){return lmbr_apply(g,0x10000,0,reason,2,B,E,0xffff800000001000ull,s,w,write);}
static void regs(uint64_t*g,uint32_t id,uint64_t v){for(unsigned i=0;i<15;i++)g[i]=0xaa00000000000000ull+i;g[2]=0xbb00000000000000ull|id;g[0]=0xdd00000000000000ull|(uint32_t)v;g[3]=0xee00000000000000ull|(uint32_t)(v>>32);}
int main(void){
 const uint32_t ids[]={0xc0000100u,0xc0000102u,0xc0000081u,0xc0000082u,0xc0000083u,0xc0000084u};
 const uint64_t values[]={0xffff800000123000ull,0xffff800000456000ull,0x0033001000000000ull,0xffff800000789000ull,0x1234000,0x200};
 for(unsigned k=0;k<6;k++){
  struct lvs_state s={0};assert(lvs_init(&s));uint64_t g[15];regs(g,ids[k],values[k]);
  struct lvs_image host=s.host;struct writes w={0};assert(call(g,&s,&w,32));
  assert(w.n==(k==0?2u:1u)&&w.f[w.n-1]==0x681e&&w.v[w.n-1]==0x10002);
  if(!k)assert(w.f[0]==0x680e&&w.v[0]==values[k]);
  else{unsigned i=0;while(lvs_msr_ids[i]!=ids[k])i++;assert(s.guest.msr[i]==values[k]);}
  assert(!memcmp(&host,&s.host,sizeof host));
  regs(g,ids[k],0);uint64_t preserved[15];memcpy(preserved,g,sizeof g);w=(struct writes){0};assert(call(g,&s,&w,31));
  uint64_t expected=k?values[k]:0xffff800000001000ull;
  assert(g[0]==(uint32_t)expected&&g[3]==(uint32_t)(expected>>32));
  for(unsigned i=0;i<15;i++)if(i!=0&&i!=3)assert(g[i]==preserved[i]);
  for(unsigned fail=1;fail<=(k==0?2u:1u);fail++){
   struct lvs_state before=s;regs(g,ids[k],0);memcpy(preserved,g,sizeof g);w=(struct writes){.fail=fail};
   assert(!call(g,&s,&w,32));assert(!memcmp(&before,&s,sizeof s)&&!memcmp(g,preserved,sizeof g));
  }
 }
 struct lvs_state s={0};assert(lvs_init(&s));uint64_t g[15];struct writes w;
 const uint32_t unsupported[]={0xc0000101u,0xc0000103u,0xc0000080u,0x277,0x480,0x174};
 for(unsigned i=0;i<6;i++){regs(g,unsupported[i],0);w=(struct writes){0};assert(!call(g,&s,&w,32)&&!w.n);}
 for(unsigned k=0;k<6;k++){
  uint64_t bad=k==2?1:(k==5?1ull<<32:1ull<<47);regs(g,ids[k],bad);w=(struct writes){0};assert(!call(g,&s,&w,32)&&!w.n);
 }
 regs(g,0xc0000082u,0);w=(struct writes){0};
 assert(!lmbr_apply(g,0x10000,0,32,2,B,1u<<29,0,&s,&w,write)&&!w.n);
 assert(!lmbr_apply(g,0x10000,0,32,2,0,E,0,&s,&w,write)&&!w.n);
 assert(!lmbr_apply(g,0x10000,3,32,2,B,E,0,&s,&w,write)&&!w.n);
 assert(!lmbr_apply(g,UINT64_MAX,0,32,2,B,E,0,&s,&w,write)&&!w.n);
 assert(!lmbr_apply(g,0x7ffffffffffeull,0,32,2,B,E,0,&s,&w,write)&&!w.n);
 assert(!lmbr_apply(g,0x10000,0,32,3,B,E,0,&s,&w,write)&&!w.n);
 s.host_dirty=true;assert(!call(g,&s,&w,32)&&!w.n);s.host_dirty=false;s.guest_valid=false;assert(!call(g,&s,&w,32)&&!w.n);
 /* Canonical endpoints and conservative STAR/SFMASK boundaries. */
 s.guest_valid=true;
 const uint64_t endpoints[]={0,0x7fffffffffffull,0xffff800000000000ull,UINT64_MAX};
 for(unsigned i=0;i<4;i++){
  regs(g,0xc0000102u,endpoints[i]);w=(struct writes){0};assert(call(g,&s,&w,32));
 }
 regs(g,0xc0000102u,0xffff7fffffffffffull);w=(struct writes){0};assert(!call(g,&s,&w,32)&&!w.n);
 regs(g,0xc0000081u,0xffffffff00000000ull);w=(struct writes){0};assert(call(g,&s,&w,32));
 regs(g,0xc0000084u,UINT32_MAX);w=(struct writes){0};assert(call(g,&s,&w,32));
 regs(g,0xc0000084u,0);uint64_t before_g[15];memcpy(before_g,g,sizeof g);
 struct lvs_state before_s=s;w=(struct writes){.fail=1};assert(!call(g,&s,&w,31));
 assert(!memcmp(g,before_g,sizeof g)&&!memcmp(&s,&before_s,sizeof s));
 w=(struct writes){0};assert(!lmbr_apply(g,0x10000,0,32,2,B,0,0,&s,&w,write)&&!w.n);
 assert(!lmbr_apply(g,0x10000,0,0x80000020ull,2,B,E,0,&s,&w,write)&&!w.n);
 s.ready=false;assert(!call(g,&s,&w,32)&&!w.n);s.ready=true;
 puts("linux_msr_bank: six MSRs, guest-only state, commit failures, capability/boundary rejects PASS");
}
