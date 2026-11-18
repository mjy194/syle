#ifndef SYLE_FIRMWARE_LIFECYCLE_H
#define SYLE_FIRMWARE_LIFECYCLE_H
#include "guest_continuous.h"
#define FMG_TOKEN "9a473c02e61f8b5d7349ab1260ec572d"
struct fml_stream {char line[256];unsigned n,match;bool overflow,ready,hash,prompt;};
static inline bool fml_equal(const char*a,unsigned n,const char*b){unsigned i=0;while(b[i]){if(i>=n||a[i]!=b[i])return false;i++;}return i==n;}
static inline unsigned fml_byte(struct fml_stream*s,uint8_t b){
 static const char ready[]="SYLE_DISK_ROOT_READY";
 if(!s->ready){if(b==(uint8_t)ready[s->match])s->match++;else s->match=b==ready[0];if(s->match==sizeof ready-1)s->ready=true;}
 else{if(s->hash&&b==' ')s->prompt=true;s->hash=b=='#';}
 if(b=='\r')return 0;
 if(b=='\n'){unsigned m=0;if(!s->overflow){if(fml_equal(s->line,s->n,"SYLE_LIFE_ARMED"))m=1;if(fml_equal(s->line,s->n,"SYLE_LIFE_RESUMED"))m=2;if(fml_equal(s->line,s->n,"SYLE_LIFE_RECREATED"))m=3;}s->n=0;s->overflow=false;return m;}
 if(s->n<sizeof s->line)s->line[s->n++]=(char)b;else s->overflow=true;return 0;
}
/* Exact response lines cannot be satisfied by the deliberately split echo
 * markers. Ring3 controls the lifecycle exclusively through existing int80. */
static inline int fml_cycle(const struct mg_continuous_ops*o,uint64_t h,bool recreate){
 static const char arm[]="m2token=764219; m2pid=$$; fbprobe && persistctl write " FMG_TOKEN " && echo SYLE_\"\"LIFE_ARMED\n";
 static const char resume[]="[ \"$m2token\" = 764219 ] && [ \"$m2pid\" = \"$$\" ] && persistctl read " FMG_TOKEN " && mount -t ext2 -o remount,ro /dev/vda2 / && echo SYLE_\"\"LIFE_RESUMED\n";
 static const char check[]="persistctl read " FMG_TOKEN " && mount -t ext2 -o remount,ro /dev/vda2 / && echo SYLE_\"\"LIFE_RECREATED\n";
 struct fml_stream stream={0};const char*command=recreate?check:arm;unsigned sent=0;bool resumed=false,running=false;
 for(unsigned step=0;step<32768;step++){
  if(o->call(o->ctx,running?SYLE_GUEST_STEP:SYLE_GUEST_START,h,0))return 1;
  running=true;
  unsigned marker=0;
  for(unsigned n=0;n<128;n++){
   int64_t b=o->call(o->ctx,SYLE_GUEST_CONSOLE_RECV,h,0);if(b==-SYLE_GUEST_EBUSY)break;
   if(b<0||b>255||!o->output(o->ctx,(uint8_t)b))return 1;
   unsigned m=fml_byte(&stream,(uint8_t)b);if(m)marker=m;
  }
  if(marker==1&&!recreate&&!resumed&&!command[sent]){
   if(o->call(o->ctx,SYLE_GUEST_STOP,h,0))return 1;
   for(unsigned n=0;n<256;n++)if(o->call(o->ctx,SYLE_GUEST_STATUS,h,0)!=SYLE_GUEST_STOPPED)return 1;
   if(o->call(o->ctx,SYLE_GUEST_STEP,h,0)!=-SYLE_GUEST_ESTATE)return 1;
   if(o->call(o->ctx,SYLE_GUEST_START,h,0))return 1;
   resumed=true;command=resume;sent=0;
  }
  if((marker==2&&resumed&&!recreate&&!command[sent])||(marker==3&&recreate&&!command[sent]))return 0;
  if(stream.prompt&&command[sent]){
   int64_t space=o->call(o->ctx,SYLE_GUEST_CONSOLE_SPACE,h,0);if(space==0)continue;
   if(space<0||space>16||o->call(o->ctx,SYLE_GUEST_CONSOLE_SEND,h,(uint8_t)command[sent]))return 1;
   sent++;
  }
 }
 return 1;
}
static inline int fml_run(const struct mg_continuous_ops*o){
 uint64_t previous=0;
 for(unsigned cycle=0;cycle<2;cycle++){
  int64_t h=o->call(o->ctx,SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
  if(h<=0){o->call(o->ctx,SYLE_GUEST_REAP,0,0);return 1;}
  int failed=(uint64_t)h==previous||fml_cycle(o,(uint64_t)h,cycle!=0);
  int64_t stop=o->call(o->ctx,SYLE_GUEST_STOP,h,0),destroy=o->call(o->ctx,SYLE_GUEST_DESTROY,h,0);
  if(failed||stop||destroy||o->call(o->ctx,SYLE_GUEST_STATUS,h,0)!=-SYLE_GUEST_ENOENT)return 1;
  previous=(uint64_t)h;
 }
 return 0;
}
#endif
