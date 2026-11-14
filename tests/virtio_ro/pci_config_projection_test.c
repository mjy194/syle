#include "pci_config_projection.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint32_t rd(struct vpc*c,struct fpm_state*p,unsigned b,unsigned o,unsigned n){uint32_t v=0;assert(vpc_config(c,p,b,o,n,false,&v)==1);return v;}
static int wt(struct vpc*c,struct fpm_state*p,unsigned b,unsigned o,unsigned n,uint32_t v){return vpc_config(c,p,b,o,n,true,&v);}
int main(void){struct vpc c={0};struct fpm_state p={0};assert(fpm_init(&p,1,0,100));
 unsigned devices[]={0,8,11};for(unsigned k=0;k<3;k++)for(unsigned n=1;n<=4;n*=2)for(unsigned o=0;o<256;o+=n){uint32_t x=rd(&c,&p,devices[k],o,n),bytes=0;for(unsigned i=0;i<n;i++)bytes|=rd(&c,&p,devices[k],o+i,1)<<(8*i);assert(x==bytes);}
 assert(rd(&c,&p,0,0,4)==0x12378086&&rd(&c,&p,8,0,4)==0x71108086&&rd(&c,&p,11,0,4)==0x71138086&&rd(&c,&p,8,14,1)==0x80);
 uint32_t v=77;assert(vpc_config(&c,&p,16,0,4,false,&v)==0&&v==77);assert(vpc_config(&c,&p,0,255,4,false,&v)==-1&&v==77);
 for(unsigned b=0;b<3;b++)for(unsigned off=16;off<40;off+=4){assert(wt(&c,&p,devices[b],off,4,UINT32_MAX)==1);assert(rd(&c,&p,devices[b],off,4)==0);}
 assert(wt(&c,&p,11,4,4,0xf900ffff)==1&&rd(&c,&p,11,4,4)==1);assert(wt(&c,&p,11,6,2,0xffff)==1&&rd(&c,&p,11,6,2)==0);
 assert(wt(&c,&p,8,4,4,UINT32_MAX)==1&&rd(&c,&p,8,4,4)==0);assert(wt(&c,&p,8,12,4,UINT32_MAX)==1&&rd(&c,&p,8,12,4)==0x800000);
 assert(wt(&c,&p,11,0x41,1,0xb0)==1&&p.pmba==0xb001);assert(wt(&c,&p,11,0x80,4,1)==1&&p.enable==1&&vpc_pm_decode(&c,&p));
 struct fpm_state old=p;struct vpc oldc=c;assert(wt(&c,&p,11,0x41,1,0)==-1&&!memcmp(&p,&old,sizeof p)&&!memcmp(&c,&oldc,sizeof c));
 assert(wt(&c,&p,11,0x80,4,0x101)==-1&&!memcmp(&p,&old,sizeof p));assert(wt(&c,&p,11,0x80,1,2)==-1&&!memcmp(&p,&old,sizeof p));
 assert(wt(&c,&p,11,4,1,0)==1&&vpc_pm_decode(&c,&p)&&p.enable==1);assert(wt(&c,&p,11,5,1,0xff)==1&&c.pm_command==0);
 assert(wt(&c,&p,11,0x80,1,0)==1&&wt(&c,&p,11,0x40,2,1)==1&&p.pmba==1);
 assert(wt(&c,&p,0,0x59,1,0x33)==-1&&wt(&c,&p,0,0x58,4,0)==1); /* PAM not invented */
 assert(wt(&c,&p,11,0x40,4,UINT32_MAX)==-1&&p.pmba==1);
 /* Live PM writes are immediately projected; no second pmba/enable copy. */
 v=0xb001;assert(fpm_config(&p,0x40,4,true,&v));assert(rd(&c,&p,11,0x40,4)==0xb001);
 puts("PCI projection widths/RO/masks/W1C-zero/live PM partial merges: host PASS");
}
