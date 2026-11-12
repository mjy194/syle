#include "linux_profile_prepare.h"
static bool copy(struct lgs_session*s,uint64_t gpa,void*buffer,size_t n,bool write){
 if(!buffer||!n||gpa>=LGM_RAM_SIZE||n>LGM_RAM_SIZE-gpa)return false;
 uint8_t*b=buffer;
 while(n){size_t part=4096-(gpa&4095);if(part>n)part=n;
 uint64_t pa=s->memory.pages[gpa/4096];if(!pa||(pa&4095)||(pa>>52))return false;
 uint8_t*p=s->memory.owner.map(s->memory.owner.context,pa);if(!p)return false;
 for(size_t i=0;i<part;i++){if(write)p[(gpa&4095)+i]=b[i];else b[i]=p[(gpa&4095)+i];}
 if(write&&!s->memory.owner.writeback(s->memory.owner.context,pa))return false;
 gpa+=part;b+=part;n-=part;
 }return true;
}
static bool read_ram(void*c,uint64_t g,void*p,size_t n){return copy(c,g,p,n,false);}
static bool write_ram(void*c,uint64_t g,const void*p,size_t n){return copy(c,g,(void*)p,n,true);}
bool lpp_init(struct lpp_context*p,uint64_t domain,void*c,bool(*now)(void*,uint64_t*)){
 if(!p||p->configured||p->busy||p->failed||!domain||!now)return false;
 uint64_t t;if(!now(c,&t))return false;
 guest_apic_init_mode(&p->apic,t,false);ioapic_init_guest(&p->io,gia_enqueue,&p->irq);
 if(!gia_init(&p->irq,&p->apic,&p->io,domain,true)||
 !lams_init(&p->transactions,&p->apic,domain,&p->irq,gia_notify)||!lio_init(&p->io_transactions,&p->irq)){
 p->failed=true;return false;}
 p->config=(struct lad_config){.transactions=&p->transactions,.irq=&p->irq,.clock_ctx=c,.now=now,.ioapic=&p->io_transactions};
 p->configured=true;return true;
}
bool lpp_prepare_guest(void*ctx,struct lgs_session*s){
 struct lpp_context*p=ctx;if(!p||!p->configured||p->failed||p->busy||p->prepared)return false;
 p->busy=true;bool ok=false;
 if(!s||p->session||s->failed||s->enabled||s->prepared||s->host.active||s->host.ready||s->host.quarantined||s->state.host_dirty||
 s->exits||s->native.resume||!s->memory.prepared||!s->memory.owner.map||!s->memory.owner.writeback||
 s->memory.plan.ram_size!=LGM_RAM_SIZE||s->memory.plan.params_addr!=0x90000||
 !lad_valid(&p->config)||!gia_exclusive(&p->irq)||p->irq.generation||p->irq.receipt_sequence||p->transactions.sequence)goto done;
 p->session=s;p->owner=s->memory.owner;p->eptp=s->memory.eptp;
 struct lai_ops ops={.context=s,.read=read_ram,.write=write_ram};
 if(!lar_install(&p->firmware,&ops,LGM_RAM_SIZE,0x90000))goto done;
 p->prepared=true;ok=true;
done:if(!ok)p->failed=true;p->busy=false;return ok;
}
