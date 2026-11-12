#include "linux_user_pf.h"
#include "linux_guest_session.h"
static bool canon(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static uint64_t u64(const uint8_t*b){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)b[i]<<(8*i);return v;}
struct walk {const struct lup_snapshot*g;uint64_t ram;void*ctx;lup_read read;};
static bool raw(struct walk*w,uint64_t a,uint64_t*v){uint8_t b[8];if(a>w->ram-8||!w->read(w->ctx,a,b,8))return false;*v=u64(b);return true;}
/* kind0 supervisor read,1 supervisor write,2 supervisor execute,3 user
 * nonpresent fetch proof. No A/D requirements: CPU may set them on delivery. */
static bool translate(struct walk*w,uint64_t va,unsigned kind,uint64_t*pa){
 if(!canon(va))return false;
 uint64_t table=w->g->cr3&UINT64_C(0xfffff000);bool us=true,rw=true,nx=false;
 for(unsigned level=0;level<4;level++){
  if(table>w->ram-4096)return false;
  uint64_t e;if(!raw(w,table+((va>>(39-9*level))&511)*8,&e))return false;
  if(!(e&1))return kind==3; /* software bits ignored only when P=0 */
  if(e&UINT64_C(0x7fffffff00000000))return false;
  us=us&&!!(e&4);rw=rw&&!!(e&2);nx=nx||!!(e>>63);
  if((nx&&!(w->g->efer&0x800))||(kind==3&&(!us||nx))||(kind==2&&nx))return false;
  if((e&128)&&level<2)return false; /* 1GiB pages not in this profile */
  if(level==3||(level==2&&(e&128))){
   if(kind==3||us||(kind==1&&!rw))return false;
   uint64_t mask=level==2?UINT64_C(0x1fffff):4095;
   if(level==2&&(e&UINT64_C(0x1fe000)))return false;
   *pa=(e&UINT64_C(0xfffff000)&~mask)|(va&mask);return *pa<w->ram;
  }
  table=e&UINT64_C(0xfffff000);
 }return false;
}
static bool range(struct walk*w,uint64_t a,unsigned n,unsigned kind,uint8_t*out){
 if(!n||a>UINT64_MAX-(n-1)||!canon(a)||!canon(a+n-1))return false;
 for(unsigned i=0;i<n;i++){uint64_t p;uint8_t scratch;if(!translate(w,a+i,kind,&p)||!w->read(w->ctx,p,out?out+i:&scratch,1))return false;}return true;
}
bool lup_validate(const struct lnp_state*s,const struct lup_snapshot*g,uint64_t ram,void*ctx,lup_read read){
 if(!s||!g||!read||ram<4096||ram>UINT64_C(0x100000000)||(ram&4095)||s->failed||s->pending||s->count>=LNP_MAX_FAULTS||
 g->reason||g->info!=0x80000b0e||g->error!=0x14||g->address!=g->rip||!canon(g->rip)||!canon(g->rsp)||
 g->event||g->entryerror||g->entrylen||g->vector||g->blocking||g->activity||g->debug||
 (g->cr0&0x80000031)!=0x80000031||(g->cr0&~UINT64_C(0x8005003b))||
 !(g->cr4&32)||(g->cr4&~UINT64_C(0x26b0))||(g->cr3&~UINT64_C(0xfffff018))||
 (g->efer&0x500)!=0x500||(g->efer&~UINT64_C(0xd01))||
 g->cs>65535||(g->cs&7)!=3||!g->cs||g->csbase||g->cslimit!=UINT32_MAX||g->csar!=0xa0fb||
 g->ss>65535||(g->ss&7)!=3||!g->ss||g->ssbase||g->sslimit!=UINT32_MAX||g->ssar!=0xc0f3||
 (g->flags&UINT64_C(0x10002))!=UINT64_C(0x10002)||(g->flags&~UINT64_C(0x210ed7))||
 g->idtlimit<239||g->idtlimit>65535||g->gdtlimit>65535||
 !canon(g->idtr)||g->idtr>UINT64_MAX-g->idtlimit||!canon(g->idtr+g->idtlimit)||
 !canon(g->gdtr)||g->gdtr>UINT64_MAX-g->gdtlimit||!canon(g->gdtr+g->gdtlimit)||
 !g->tr||g->tr>65535||(g->tr&7)||g->trar!=0x8b||g->trlimit<103||g->trlimit>65535||!canon(g->trbase)||g->trbase>UINT64_MAX-g->trlimit||!canon(g->trbase+g->trlimit))return false;
 for(unsigned i=0;i<s->count;i++)if(s->history[i].rip==g->rip&&s->history[i].address==g->address&&s->history[i].cr3==g->cr3&&s->history[i].error==g->error)return false;
 struct walk w={g,ram,ctx,read};uint64_t unused;
 if(!translate(&w,g->address,3,&unused))return false;
 uint8_t gate[16],desc[16],slot[8];
 if(!range(&w,g->idtr+224,16,0,gate))return false;
 uint64_t lo=u64(gate),hi=u64(gate+8),sel=(lo>>16)&65535;
 if(((lo>>32)&65535)!=0x8e00||(hi>>32)||!sel||(sel&7)||sel>g->gdtlimit||g->gdtlimit-sel<7)return false;
 if(!range(&w,g->gdtr+sel,8,0,desc))return false;
 uint64_t cs=u64(desc);if(cs!=UINT64_C(0x00af9b000000ffff))return false;
 if(g->tr>g->gdtlimit||g->gdtlimit-g->tr<15||!range(&w,g->gdtr+g->tr,16,0,desc))return false;
 lo=u64(desc);hi=u64(desc+8);
 uint64_t base=((lo>>16)&UINT64_C(0xffffff))|((lo>>32)&UINT64_C(0xff000000))|(hi<<32);
 uint64_t limit=(lo&65535)|((lo>>32)&0xf0000);
 if((hi>>32)||((lo>>40)&255)!=0x8b||(lo&UINT64_C(0x00f0000000000000))||base!=g->trbase||limit!=g->trlimit)return false;
 if(!range(&w,g->trbase+4,8,0,slot))return false;
 uint64_t stack=u64(slot),top=stack&~UINT64_C(15);
 if(!canon(stack)||top<48)return false;
 lo=u64(gate);hi=u64(gate+8);
 uint64_t handler=(lo&65535)|((lo>>32)&UINT64_C(0xffff0000))|(hi<<32);
 return range(&w,handler,16,2,0)&&range(&w,top-48,48,1,0);
}
static bool owned(void*x,uint64_t a,void*out,size_t n){
 struct lgs_session*s=x;if(!n||a>=LGM_RAM_SIZE||n>LGM_RAM_SIZE-a)return false;
 uint8_t*b=out;for(size_t i=0;i<n;i++){uint64_t p=s->memory.pages[(a+i)>>12];if(!p||(p&4095)||(p>>52))return false;const uint8_t*q=s->memory.owner.map(s->memory.owner.context,p);if(!q)return false;b[i]=q[(a+i)&4095];}return true;
}
bool lup_inject(struct lnp_state*st,struct lgs_session*s){
 if(!st||st->failed)return false;
 if(!s||!s->prepared||s->failed||!s->memory.prepared||!s->memory.owner.map||!s->host.active||!s->host.ready||!s->state.ready||!s->state.guest_valid||s->state.host_dirty||!s->platform.read_field||!s->hop.vmwrite)goto fail;
 struct lup_snapshot g;
 uint64_t*dst[]={&g.reason,&g.info,&g.error,&g.address,&g.rip,&g.rsp,&g.cr0,&g.cr3,&g.cr4,&g.efer,&g.cs,&g.csbase,&g.cslimit,&g.csar,&g.ss,&g.ssbase,&g.sslimit,&g.ssar,&g.idtr,&g.idtlimit,&g.gdtr,&g.gdtlimit,&g.tr,&g.trbase,&g.trlimit,&g.trar,&g.flags,&g.debug,&g.event,&g.entryerror,&g.entrylen,&g.vector,&g.blocking,&g.activity};
 const uint32_t fields[]={0x4402,0x4404,0x4406,0x6400,0x681e,0x681c,0x6800,0x6802,0x6804,0x2806,0x0802,0x6808,0x4802,0x4816,0x0804,0x680a,0x4804,0x4818,0x6818,0x4812,0x6816,0x4810,0x080e,0x6814,0x480e,0x4822,0x6820,0x6822,0x4016,0x4018,0x401a,0x4408,0x4824,0x4826};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)if(!s->platform.read_field(s->platform.ctx,fields[i],dst[i]))goto fail;
 if(!lup_validate(st,&g,LGM_RAM_SIZE,s,owned))goto fail;
 if(!s->hop.vmwrite(s->hop.context,0x4018,g.error)||!s->hop.vmwrite(s->hop.context,0x401a,0)||!s->hop.vmwrite(s->hop.context,0x4016,0x80000b0e))goto fail;
 st->history[st->count++]=(struct lnp_tuple){g.rip,g.address,g.cr3,g.error};st->pending=true;s->native.cr2=g.address;return true;
fail:st->failed=true;return false;
}
