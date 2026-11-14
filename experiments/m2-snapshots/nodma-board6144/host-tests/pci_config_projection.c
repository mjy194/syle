#include "pci_config_projection.h"
static uint8_t byte(const struct vpc*c,const struct fpm_state*p,unsigned bdf,unsigned o){
 uint32_t d=0;unsigned base=o&~3u;
 switch(base){
 case 0:d=bdf==0?0x12378086u:bdf==8?0x71108086u:0x71138086u;break;
 case 4:d=bdf==11?c->pm_command:0;break;
 case 8:d=bdf==0?0x06000002u:bdf==8?0x06010000u:0x06800000u;break;
 case 12:d=bdf==8?0x00800000u:0;break;
 case 0x40:if(bdf==11)d=p->pmba;break;
 case 0x80:if(bdf==11)d=p->enable;break;
 default:break;
 }return (uint8_t)(d>>(8*(o&3)));
}
int vpc_config(struct vpc*c,struct fpm_state*p,uint16_t bdf,unsigned o,unsigned n,bool write,uint32_t*v){
 if(bdf!=0&&bdf!=8&&bdf!=11)return 0;
 if(!c||!p||!v||!p->ready||p->failed||(n!=1&&n!=2&&n!=4)||o>=256||n>256-o||o%n)return -1;
 if(!write){uint32_t result=0;for(unsigned i=0;i<n;i++)result|=(uint32_t)byte(c,p,bdf,o+i)<<(8*i);*v=result;return 1;}
 struct vpc next=*c;struct fpm_state pm=*p;uint32_t pmba=p->pmba,enable=p->enable;bool setbase=false,setenable=false;
 for(unsigned i=0;i<n;i++){
  unsigned at=o+i;uint8_t value=(uint8_t)(*v>>(8*i));
  if(bdf==11&&at==4){next.pm_command=value&1;continue;}
  if(at<0x40)continue; /* fixed/RO/unimplemented header and no latched errors */
  if(bdf==11&&at>=0x40&&at<=0x43){unsigned shift=(at-0x40)*8;pmba=(pmba&~(0xffu<<shift))|((uint32_t)value<<shift);setbase=true;continue;}
  if(bdf==11&&at==0x80){enable=value;setenable=true;continue;}
  if(value)return -1; /* no invented chipset side effects */
 }
 if(setbase&&!fpm_config(&pm,0x40,4,true,&pmba))return -1;
 if(setenable&&!fpm_config(&pm,0x80,1,true,&enable))return -1;
 *c=next;*p=pm;return 1;
}
bool vpc_pm_decode(const struct vpc*c,const struct fpm_state*p){return c&&p&&p->ready&&!p->failed&&p->enable==1&&p->pmba==0xb001;}
