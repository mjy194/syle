#ifndef SYLE_GUEST_RESUME_PROBE_H
#define SYLE_GUEST_RESUME_PROBE_H
#include "guest_continuous.h"
#define MG_RESUME_ARM "syle_resume_token=764219; syle_resume_pid=$$; printf '\\n'; echo SYLE_\"\"RESUME_ARMED\n"
#define MG_RESUME_CHECK "[ \"$syle_resume_token\" = 764219 ] && [ \"$syle_resume_pid\" = \"$$\" ] && { printf '\\n'; echo SYLE_\"\"RESUME_OK; }\n"
enum mg_resume_result {MG_RESUME_VERIFIED,MG_RESUME_TIMEOUT,MG_RESUME_FAILED,MG_RESUME_CLEANUP_FAILED};
struct mg_resume_report {uint32_t slices,sent;bool ready,armed,resumed,verified;int64_t guest_result;};
struct mg_resume_stream {char line[256];unsigned length,ready_match;bool overflow,ready,prompt_hash,prompt;};
static inline bool mr_equal(const char*a,unsigned n,const char*b){unsigned i=0;while(b[i]){if(i>=n||a[i]!=b[i])return false;i++;}return i==n;}
/* Exact newline-delimited markers; command echoes deliberately split them. */
static inline unsigned mr_byte(struct mg_resume_stream*s,uint8_t b){
 static const char ready[]="SYLE_LINUX_USERSPACE_READY";
 if(!s->ready){if(b==(uint8_t)ready[s->ready_match])s->ready_match++;else s->ready_match=b==ready[0]?1:0;
  if(s->ready_match==sizeof(ready)-1)s->ready=true;}
 else {if(s->prompt_hash&&b==' ')s->prompt=true;s->prompt_hash=b=='#';}
 if(b=='\r')return 0;
 if(b=='\n'){unsigned result=0;if(!s->overflow){if(mr_equal(s->line,s->length,"SYLE_RESUME_ARMED"))result=1;
   if(mr_equal(s->line,s->length,"SYLE_RESUME_OK"))result=2;}
  s->length=0;s->overflow=false;return result;}
 if(s->length<sizeof(s->line))s->line[s->length++]=(char)b;else s->overflow=true;
 return 0;
}
static inline enum mg_resume_result mg_resume_run_with_pause(const struct mg_continuous_ops*o,uint32_t budget,uint32_t drain,uint32_t post,struct mg_resume_report*r,const char*arm,const char*check,bool(*pause)(void*,uint64_t)){
 if(!o||!o->call||!o->output||!budget||!drain||!post||!r||!arm||!check)return MG_RESUME_FAILED;
 *r=(struct mg_resume_report){0};
 int64_t h=o->call(o->ctx,SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
 if(h<=0)return o->call(o->ctx,SYLE_GUEST_REAP,0,0)?MG_RESUME_CLEANUP_FAILED:MG_RESUME_FAILED;
 enum mg_resume_result result=MG_RESUME_TIMEOUT;
 struct mg_resume_stream stream={0};unsigned phase=0,sent=0,continued=0;bool running=false;
 const char*cmd=arm;
 for(uint32_t i=0;i<budget;i++){
  r->slices=i+1;r->guest_result=o->call(o->ctx,running?SYLE_GUEST_STEP:SYLE_GUEST_START,(uint64_t)h,0);
  if(r->guest_result){result=MG_RESUME_FAILED;break;}running=true;
  bool verified_before=r->verified;
  for(uint32_t n=0;n<drain;n++){
   int64_t b=o->call(o->ctx,SYLE_GUEST_CONSOLE_RECV,(uint64_t)h,0);
   if(b==-SYLE_GUEST_EBUSY||b==-16)break;
   if(b<0||b>255||!o->output(o->ctx,(uint8_t)b)){result=MG_RESUME_FAILED;break;}
   unsigned marker=mr_byte(&stream,(uint8_t)b);
   if(marker==1&&phase==0&&!cmd[sent])r->armed=true;
   if(marker==2&&phase==1&&!cmd[sent])r->verified=true;
  }
  r->ready=stream.ready;
  if(result==MG_RESUME_FAILED)break;
  if(r->armed&&phase==0){
   /* No destroy/create, and never advance the guest while stopped. */
   r->guest_result=o->call(o->ctx,SYLE_GUEST_STOP,(uint64_t)h,0);
   if(r->guest_result){result=MG_RESUME_FAILED;break;}
   if(pause&&!pause(o->ctx,(uint64_t)h)){result=MG_RESUME_FAILED;break;}
   r->guest_result=o->call(o->ctx,SYLE_GUEST_START,(uint64_t)h,0);
   if(r->guest_result){result=MG_RESUME_FAILED;break;}
   r->resumed=true;phase=1;cmd=check;sent=0;
   /* Prompt may already have been drained with the ARMED line. The shell
    * consumes buffered serial input after resume even if prompt is pending. */
  }
  if(verified_before&&++continued>=post){result=MG_RESUME_VERIFIED;break;}
  if(stream.prompt&&!cmd[sent])continue;
  if(stream.prompt){
   int64_t space=o->call(o->ctx,SYLE_GUEST_CONSOLE_SPACE,(uint64_t)h,0);
   if(space==-SYLE_GUEST_EBUSY||space==0)continue;
   if(space<0||space>16){result=MG_RESUME_FAILED;break;}
   if(o->call(o->ctx,SYLE_GUEST_CONSOLE_SEND,(uint64_t)h,(uint8_t)cmd[sent])){result=MG_RESUME_FAILED;break;}
   sent++;r->sent++;
  }
 }
 int64_t stop=o->call(o->ctx,SYLE_GUEST_STOP,(uint64_t)h,0);
 int64_t destroy=o->call(o->ctx,SYLE_GUEST_DESTROY,(uint64_t)h,0);
 return stop||destroy?MG_RESUME_CLEANUP_FAILED:result;
}
static inline enum mg_resume_result mg_resume_run(const struct mg_continuous_ops*o,uint32_t budget,uint32_t drain,uint32_t post,struct mg_resume_report*r){
 return mg_resume_run_with_pause(o,budget,drain,post,r,MG_RESUME_ARM,MG_RESUME_CHECK,0);
}
#endif
