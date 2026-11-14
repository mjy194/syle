#include "linux_pic04_domain.h"
/* 16550 baud = 115200/divisor. Use half bits for 5-bit/1.5-stop
 * frames: delta PIT clocks * 230400 / (1193182 * divisor * half_bits).
 * No 128-bit division or unbounded product in the freestanding kernel. */
static uint64_t uart_period(const struct linux_uart*u){
 unsigned divisor=((unsigned)u->dlm<<8)|u->dll;
 unsigned data=5+(u->lcr&3);
 unsigned half_bits=2+2*data+((u->lcr&8)?2:0)+((u->lcr&4)?(data==5?3:4):2);
 return UINT64_C(1193182)*divisor*half_bits;
}
static bool uart_clock(struct lpd_domain*d,uint64_t delta){
 struct linux_uart*u=&d->uart.uart;uint64_t period=uart_period(u);
 if(!period||!u->rc||u->rx_idle==4){d->uart_fraction=0;return lurt_tick(&d->uart,0)==LURT_COMMITTED;}
 uint64_t need=(4-u->rx_idle)*period-d->uart_fraction;
 unsigned chars;
 if(delta>=need/230400+(need%230400!=0)){chars=4;d->uart_fraction=0;}
 else {uint64_t units=delta*230400+d->uart_fraction;chars=(unsigned)(units/period);d->uart_fraction=units%period;}
 return lurt_tick(&d->uart,chars)==LURT_COMMITTED;
}
static bool canon(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool irq4(void*p,unsigned irq,bool level){struct lpd_domain*d=p;return d&&irq==4&&pm04_level4(&d->master,d->master.domain,level);}
void lpd_bind(struct lpd_domain*d){if(d){d->uart.irq_context=d;d->uart.sync_irq=irq4;}}
bool lpd_valid(const struct lpd_domain*d){
 if(!d||d->failed||d->master.quarantined||d->slave.failed||!d->uart.ready||d->uart.busy||d->uart.irq_context!=d||d->uart.sync_irq!=irq4||d->master.seen0!=d->timer.edges||d->slave.master!=255)return false;
 
 uint64_t period=uart_period(&d->uart.uart);
 if((!period&&d->uart_fraction)||(period&&d->uart_fraction>=period)||(!d->uart.uart.rc&&d->uart_fraction))return false;
 if(!d->uart.irq_dirty&&d->master.level4!=linux_uart_irq_level(&d->uart.uart))return false;
 const struct lpic_chip*c=&d->slave.chip[0];if(c->phase||c->vector||c->cascade||c->started||c->aeoi)return false;
 
 struct pic_master_04 m=d->master;struct pit0 t=d->timer;struct linux_uart u=d->uart.uart;uint8_t v=0;
 return pm04_edges0(&m,m.domain,m.seen0)&&pit0_advance(&t,t.tick)&&linux_uart_access(&u,3,false,&v);
}
bool lpd_init(struct lpd_domain*d,uint64_t domain){if(!d||!domain)return false;
 *d=(struct lpd_domain){0};pit0_init(&d->timer);lpic_init(&d->slave);if(!pm04_init(&d->master,domain,0,false)||!lurt_init(&d->uart,d,irq4))return false;
 return lpd_valid(d);}
static bool idle(const struct lpd_domain*d){return lpd_valid(d)&&!d->master.reserved.serial;}
static void commit(struct lpd_domain*d,const struct lpd_domain*n){*d=*n;lpd_bind(d);}
bool lpd_sync(struct lpd_domain*d){if(!idle(d))return false;
 struct lpd_domain n=*d;lpd_bind(&n);if(lurt_retry_irq(&n.uart)!=LURT_COMMITTED||!lpd_valid(&n))return false;
 commit(d,&n);return true;}
bool lpd_entry_ok(const struct lpd_domain*d,uint64_t flags){return lpd_valid(d)&&lpic_entry_ok(&d->slave,flags)&&!((d->master.phase||d->master.mask_pending)&&(flags&512))&&((d->master.imr&4)||d->master.mask_pending||d->slave.slave==255);}
bool lpd_tick(struct lpd_domain*d,uint64_t tick){if(!idle(d))return false;
 struct lpd_domain n=*d;lpd_bind(&n);if(tick<n.timer.tick||!uart_clock(&n,tick-n.timer.tick)||!pit0_advance(&n.timer,tick)||!pm04_edges0(&n.master,n.master.domain,n.timer.edges)||!lpd_sync(&n))return false;
 commit(d,&n);return true;}
static bool io_ok(uint64_t g[15],uint64_t rip,uint64_t q,unsigned len){return g&&!(q&~UINT64_C(0xffff0048))&&((q&64)?len==2:(len==1&&(uint16_t)g[3]==(q>>16)))&&canon(rip)&&rip<=UINT64_MAX-len&&canon(rip+len);}
struct capture{unsigned count;uint64_t rip;};
static bool cap(void*p,uint32_t f,uint64_t v){struct capture*c=p;if(f!=0x681e||c->count++)return false;
 c->rip=v;return true;}
bool lpd_ports(struct lpd_domain*d,uint64_t g[15],uint64_t rip,uint64_t q,unsigned len,uint64_t flags,void*x,bool(*w)(void*,uint32_t,uint64_t)){
 if(!idle(d)||!w||!io_ok(g,rip,q,len)||!lpd_entry_ok(d,flags))return false;
 
 unsigned port=q>>16;bool input=q&8;if(port!=0x20&&port!=0x21&&port!=0xa0&&port!=0xa1)return false;
 
 struct lpd_domain n=*d;lpd_bind(&n);uint64_t regs[15];for(unsigned i=0;i<15;i++)regs[i]=g[i];
 if(port<0xa0){uint8_t v=g[0];if(!input&&port==0x20&&v==17&&(flags&512))return false;
 
  if(!pm04_access(&n.master,port,!input,&v,n.slave.slave==255))return false;
 if(input)regs[0]=(regs[0]&~UINT64_C(255))|v;
 }else{struct capture c={0};if(!lpic_io_flags(&n.slave,regs,rip,0,30,q,len,flags,&c,cap)||c.count!=1||c.rip!=rip+len)return false;
 }
 if(!lpd_entry_ok(&n,flags)||!w(x,0x681e,rip+len))return false;
 commit(d,&n);for(unsigned i=0;i<15;i++)g[i]=regs[i];return true;
}
bool lpd_pit_io(struct lpd_domain*d,uint64_t g[15],uint64_t rip,uint64_t q,unsigned len,uint64_t tick,void*x,bool(*w)(void*,uint32_t,uint64_t)){
 if(!idle(d)||!w||!io_ok(g,rip,q,len))return false;
 unsigned port=q>>16;if(port!=0x40&&port!=0x43)return false;
 
 struct lpd_domain n=*d;lpd_bind(&n);uint8_t v=g[0];if(!lpd_tick(&n,tick)||!pit0_access(&n.timer,tick,port,!(q&8),&v)||!pm04_edges0(&n.master,n.master.domain,n.timer.edges)||!w(x,0x681e,rip+len))return false;
 
 commit(d,&n);if(q&8)g[0]=(g[0]&~UINT64_C(255))|v;return true;
}
bool lpd_uart_io(struct lpd_domain*d,uint64_t g[15],uint64_t rip,uint64_t q,unsigned len,void*x,bool(*w)(void*,uint32_t,uint64_t)){
 if(!idle(d)||!g||!w)return false;
 struct lpd_domain n=*d;lpd_bind(&n);uint64_t regs[15];for(unsigned i=0;i<15;i++)regs[i]=g[i];
 if(lurt_guest_io(&n.uart,regs,rip,0,30,q,len,x,w)!=LURT_COMMITTED)return false;
 const struct linux_uart*old=&d->uart.uart;struct linux_uart*now=&n.uart.uart;
 unsigned reg=(unsigned)(q>>16)-0x3f8;bool write=!(q&8),dlab=!!(old->lcr&128);
 bool activity=reg==0&&!dlab&&((!write&&old->rc)||(write&&(old->mcr&16)));
 bool config=old->dll!=now->dll||old->dlm!=now->dlm||((old->lcr^now->lcr)&0x3f)||old->fcr!=now->fcr;
 bool reset_fifo=write&&reg==2&&(g[0]&2);
 if(activity||config||reset_fifo||!now->rc){n.uart_fraction=0;now->rx_idle=0;}
 if(lurt_retry_irq(&n.uart)!=LURT_COMMITTED||!lpd_valid(&n))return false;
 
 commit(d,&n);for(unsigned i=0;i<15;i++)g[i]=regs[i];return true;
}
bool lpd_send(struct lpd_domain*d,uint8_t b,bool*accepted){if(!idle(d)||!accepted)return false;
 struct lpd_domain n=*d;lpd_bind(&n);bool a=false;if(lurt_receive(&n.uart,b,&a)!=LURT_COMMITTED)return false;
 n.uart_fraction=0;if(!lpd_valid(&n))return false;
 commit(d,&n);*accepted=a;return true;}
bool lpd_recv(struct lpd_domain*d,bool*available,uint8_t*byte){if(!idle(d)||!available||!byte)return false;
 struct lpd_domain n=*d;lpd_bind(&n);bool a=false;uint8_t b=0;if(lurt_pop_tx(&n.uart,&a,&b)!=LURT_COMMITTED||!lpd_valid(&n))return false;
 commit(d,&n);*available=a;*byte=b;return true;}
