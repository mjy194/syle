#include "../linux_host_clock.h"
#include "../linux_pic04_domain.h"
#include <assert.h>
#include <stdio.h>
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;(void)v;return f==0x681e;}
static void io(struct lpd_domain*d,unsigned reg,uint8_t v){uint64_t g[15]={v};g[3]=0x3f8+reg;assert(lpd_uart_io(d,g,0x1000,g[3]<<16,1,0,wr));}
static struct lhc_sample sample(uint64_t t){return (struct lhc_sample){t,1,0};}
int main(void){
 struct lhc_clock c;struct lhc_source src={3000000000,1,0,true,true};
 assert(lhc_init(&c,&src,sample(0),3000000000));assert(lhc_start(&c,sample(0)));
 struct lpd_domain d;assert(lpd_init(&d,1));io(&d,3,0x83);io(&d,0,1);io(&d,1,0);io(&d,3,3);io(&d,2,0xc1);io(&d,1,1);io(&d,4,8);
 bool accepted=false;assert(lpd_send(&d,65,&accepted)&&accepted);
 /* Four 8N1 characters: timeout after ~347us, independently of sample count. */
 assert(lhc_advance(&c,sample(1000000)));assert(lpd_tick(&d,c.tick));assert(!d.master.level4);
 assert(lhc_stop(&c,sample(1000000)));assert(lhc_start(&c,sample(3000000000)));assert(lpd_tick(&d,c.tick)&&!d.master.level4);
 assert(lhc_advance(&c,sample(3000100000)));assert(lpd_tick(&d,c.tick)&&d.master.level4&&d.timer.tick==c.tick);
 assert(lhc_advance(&c,sample(3000100000)));assert(lpd_tick(&d,c.tick)&&d.master.level4); /* repeated same tick allowed */
 puts("host-clock/PIT-domain/UART timeout/pause common tick PASS");return 0;
}
