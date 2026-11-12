#include "pic0_ports.h"
static bool canon(uint64_t v){return (v>>47)==0||(v>>47)==0x1ffff;}
static bool state_ok(const struct pcp*s,uint64_t flags){
 if(!s||!s->master||s->failed)return false;
 struct pic0 m=*s->master;
 if(!pic0_edges(&m,m.domain,m.seen)||s->slave.master!=255)return false;
 const struct lpic_chip*c=&s->slave.chip[0];
 if(c->phase||c->vector||c->cascade||c->started||c->aeoi)return false;
 if(!lpic_entry_ok(&s->slave,flags))return false;
 if((m.phase||m.mask_pending)&&(flags&512))return false;
 /* A cascade line can only be exposed when the empty slave is masked. */
 if(!(m.imr&4)&&!m.mask_pending&&s->slave.slave!=255)return false;
 return true;
}
bool pcp_entry_ok(const struct pcp*s,uint64_t flags){return s&&!s->busy&&state_ok(s,flags);}
bool pcp_init(struct pcp*s,struct pic0*m){
 if(!s||!m)return false;
 struct pcp n={.master=m};lpic_init(&n.slave);
 if(!state_ok(&n,0)||m->reserved)return false;
 *s=n;return true;
}
struct capture {uint64_t rip;unsigned calls;};
static bool capture(void*c,uint32_t field,uint64_t value){struct capture*x=c;if(field!=0x681e||x->calls++)return false;x->rip=value;return true;}
bool pcp_io(struct pcp*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,uint64_t flags,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||s->failed||s->busy)return false;
 s->busy=true;bool ok=false;
 if(!g||!write||!state_ok(s,flags)||s->master->reserved||cpl||reason!=30||(q&~UINT64_C(0xffff0048))||!canon(rip)||rip>UINT64_MAX-len||!canon(rip+len))goto done;
 unsigned port=q>>16;bool input=q&8,imm=q&64;
 if((port!=0x20&&port!=0x21&&port!=0xa0&&port!=0xa1)||(imm?len!=2:(len!=1||(uint16_t)g[3]!=port)))goto done;
 struct pic0 master=*s->master;struct pcp n=*s;n.master=&master;
 uint64_t regs[15];for(unsigned i=0;i<15;i++)regs[i]=g[i];
 if(port<0xa0){uint8_t value=g[0];
  if(!input&&port==0x20&&value==0x11&&(flags&512))goto done;
  if(!pic0_access(&master,port,!input,&value))goto done;
  if(input)regs[0]=(regs[0]&~UINT64_C(255))|value;
 }else{
  struct capture cap={0};
  if(!lpic_io_flags(&n.slave,regs,rip,cpl,reason,q,len,flags,&cap,capture)||cap.calls!=1||cap.rip!=rip+len)goto done;
 }
 if(!state_ok(&n,flags)||!write(ctx,0x681e,rip+len))goto done;
 *s->master=master;s->slave=n.slave;for(unsigned i=0;i<15;i++)g[i]=regs[i];ok=true;
 done:s->busy=false;if(!ok)s->failed=true;return ok;
}
