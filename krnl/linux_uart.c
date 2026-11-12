#include "linux_uart.h"
/* Legal configuration latch is separate from effective byte TX support.
 * Break has immediate line effects and is intentionally not implemented. */
static bool lcr_supported(uint8_t v){return !(v&0x40);}
static bool valid(const struct linux_uart*u){return u&&u->rh<16&&u->th<16&&u->rc<=16&&u->tc<=16&&u->rx_idle<=4&&!(u->modem&15)&&!(u->modem_external&15)&&!(u->modem_delta&0xf0)&&!(u->ier&~15)&&!(u->mcr&0xe0)&&!(u->fcr&0x3e)&&lcr_supported(u->lcr);}
static unsigned capacity(const struct linux_uart*u){return (u->fcr&1)?16:1;}
static unsigned trigger(const struct linux_uart*u){static const unsigned n[]={1,4,8,14};return (u->fcr&1)?n[u->fcr>>6]:1;}
static uint8_t irq_id(const struct linux_uart*u){if((u->ier&4)&&u->overrun)return 6;if((u->ier&1)&&u->rc>=trigger(u))return 4;if((u->ier&1)&&(u->fcr&1)&&u->rc&&u->rx_idle>=4)return 12;if((u->ier&2)&&u->tx_pending&&u->tc==0)return 2;if((u->ier&8)&&u->modem_delta)return 0;return 1;}
static void modem_update(struct linux_uart*u,uint8_t now){
 uint8_t diff=u->modem^now;
 u->modem_delta|=((diff>>4)&3)|((diff>>4)&8)|(((u->modem&0x40)&&!(now&0x40))?4:0);
 u->modem=now;
}
static uint8_t loop_modem(uint8_t m){return ((m&2)?0x10:0)|((m&1)?0x20:0)|((m&4)?0x40:0)|((m&8)?0x80:0);}
bool linux_uart_modem_input(struct linux_uart*u,uint8_t v){if(!valid(u)||(v&15))return false;u->modem_external=v;if(!(u->mcr&16))modem_update(u,v);return true;}
bool linux_uart_tick(struct linux_uart*u,unsigned n){if(!valid(u))return false;if(u->rc){unsigned room=4-u->rx_idle;u->rx_idle+=n<room?n:room;}else u->rx_idle=0;return true;}
void linux_uart_init(struct linux_uart*u){if(u)*u=(struct linux_uart){0};}
bool linux_uart_irq_level(const struct linux_uart*u){return valid(u)&&(u->mcr&8)&&irq_id(u)!=1;}
bool linux_uart_publish_irq(const struct linux_uart*u,void*x,bool(*set)(void*,bool)){return valid(u)&&set&&set(x,linux_uart_irq_level(u));}
bool linux_uart_receive(struct linux_uart*u,uint8_t b,bool*accepted){
 if(!valid(u)||!accepted||(u->mcr&16))return false;
 u->rx_idle=0;
 if(u->rc>=capacity(u)){u->overrun=true;*accepted=false;return true;}
 u->rx[(u->rh+u->rc)%16]=b;u->rc++;*accepted=true;return true;
}
bool linux_uart_drain(struct linux_uart*u,uint8_t*b){
 if(!valid(u)||!b||!u->tc)return false;
 *b=u->tx[u->th];u->th=(u->th+1)%16;u->tc--;
 if(!u->tc)u->tx_pending=true;
 return true;
}
bool linux_uart_access(struct linux_uart*u,unsigned reg,bool write,uint8_t*v){
 if(!valid(u)||!v||reg>7)return false;
 struct linux_uart n=*u;uint8_t r=*v;bool dlab=!!(n.lcr&128);
 if(write){switch(reg){
 case 0:if(dlab)n.dll=r;else{if((n.lcr&~0x30u)!=3)return false;if(n.mcr&16){n.rx_idle=0;if(n.rc>=capacity(&n))n.overrun=true;else{n.rx[(n.rh+n.rc)%16]=r;n.rc++;}n.tx_pending=true;}else{if(n.tc>=capacity(&n))return false;n.tx[(n.th+n.tc)%16]=r;n.tc++;n.tx_pending=false;}}break;
 case 1:if(dlab)n.dlm=r;else{if(r&~15)return false;if((r&2)&&!(n.ier&2)&&!n.tc)n.tx_pending=true;n.ier=r;}break;
 case 2:if(r&0x38)return false;if((r&1)!=(n.fcr&1)){n.rh=n.rc=n.th=n.tc=0;n.rx_idle=0;n.tx_pending=true;}if(r&2){n.rh=n.rc=0;n.rx_idle=0;n.overrun=false;}if(r&4){n.th=n.tc=0;n.tx_pending=true;}n.fcr=r&0xc1;break;
 case 3:if(!lcr_supported(r))return false;n.lcr=r;break;
 case 4:if(r&0xe0)return false;if(((r^n.mcr)&16)&&n.tc)return false;n.mcr=r;modem_update(&n,(r&16)?loop_modem(r):n.modem_external);break;
 case 5:case 6:return false;
 case 7:n.scr=r;break;
 }}else{switch(reg){
 case 0:if(dlab)r=n.dll;else if(n.rc){r=n.rx[n.rh];n.rh=(n.rh+1)%16;n.rc--;n.rx_idle=0;}else r=0;break;
 case 1:r=dlab?n.dlm:n.ier;break;
 case 2:r=irq_id(&n);if(r==2)n.tx_pending=false;r|=(n.fcr&1)?0xc0:0;break;
 case 3:r=n.lcr;break;case 4:r=n.mcr;break;
 case 5:r=(n.rc?1:0)|(n.overrun?2:0)|(n.tc?0:0x60);n.overrun=false;break;
 case 6:r=n.modem|n.modem_delta;n.modem_delta=0;break;case 7:r=n.scr;break;
 }}
 *u=n;*v=r;return true;
}
