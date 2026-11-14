#include "linux_rtc.h"
#define HZ UINT64_C(1193182)
#define SECONDS UINT64_C(3155760000) /* 2000..2099 inclusive, 25 leap years. */
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static uint8_t bcd(unsigned x){return (uint8_t)((x/10)*16+x%10);}
static uint64_t periodic(uint64_t t){return (t/HZ)*1024+(t%HZ)*1024/HZ;}
void lrtc_init(struct lrtc_state*s){if(s)*s=(struct lrtc_state){0};}
static bool read_data(struct lrtc_state*s,uint8_t*out){
 uint64_t sec=s->tick/HZ,days=sec/86400;unsigned day=(unsigned)days,year=2000;
 static const unsigned md[]={31,28,31,30,31,30,31,31,30,31,30,31};
 while(day>=(year%4?365u:366u)){day-=year%4?365u:366u;year++;}
 unsigned month=0;while(day>=md[month]+(month==1&&year%4==0)){day-=md[month]+(month==1&&year%4==0);month++;}
 switch(s->index){
 case 0:*out=bcd(sec%60);break;case 2:*out=bcd(sec/60%60);break;case 4:*out=bcd(sec/3600%24);break;
 case 6:*out=bcd((days+6)%7+1);break;case 7:*out=bcd(day+1);break;case 8:*out=bcd(month+1);break;case 9:*out=bcd(year%100);break;
 #ifdef LRTC_FIRMWARE_COLD_BOOT
 /* Explicit firmware cold-reset profile: no S3 resume/shutdown payload.
  * Private zero shutdown byte with idempotent clear only; never host CMOS access. */
 case 15:*out=0;break;
 /* Firmware wrapper admits only owned-layout RAM [0,128MiB).
  * 0x0700 units of 64KiB above 16MiB, no RAM above 4GiB.
  * These are size descriptors, not a claim that low legacy holes are usable. */
 case 0x34:case 0x5b:case 0x5c:case 0x5d:*out=0;break;
 case 0x35:*out=7;break;
#endif
 case 10:*out=0x26|((s->tick%HZ)>=HZ-292?0x80:0);break;
 case 11:*out=2;break;case 12:*out=s->status_c;s->status_c=0;break;case 13:*out=0x80;break;
 default:return false;
 }return true;
}
bool lrtc_io(struct lrtc_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,uint64_t tick,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||s->failed)return false;
 bool ok=false;
 if(!g||!write||s->index>127||(s->status_c&~0x50)||tick<s->tick||tick>=SECONDS*HZ||cpl||reason!=30||
 (q&~UINT64_C(0xffff0048))||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len))goto done;
 unsigned port=q>>16;bool input=q&8,immediate=q&64;
 if((port!=0x70&&port!=0x71)||(immediate?len!=2:(len!=1||(uint16_t)g[3]!=port)))goto done;
 struct lrtc_state next=*s;uint8_t value=(uint8_t)g[0];
 if(tick/HZ>s->tick/HZ)next.status_c|=0x10;
 if(periodic(tick)>periodic(s->tick))next.status_c|=0x40;
 next.tick=tick;
 if(port==0x70){if(input)goto done;next.index=value&127;next.nmi_mask=!!(value&128);}
 else if(input){if(!read_data(&next,&value))goto done;}
 else if(!((next.index==10&&value==0x26)||(next.index==11&&value==2)
#ifdef LRTC_FIRMWARE_COLD_BOOT
 /* PlatformPei clears the shutdown byte after testing for S3 (0xfe).
  * This cold-only profile cannot store or initiate a resume payload. */
 ||(next.index==15&&value==0)
#endif
 ))goto done;
 if(!write(ctx,0x681e,rip+len))goto done;
 *s=next;if(input)g[0]=(g[0]&~UINT64_C(255))|value;
 ok=true;
done:if(!ok)s->failed=true;return ok;
}
