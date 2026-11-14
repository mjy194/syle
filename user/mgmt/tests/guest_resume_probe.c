#include "../guest_resume_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct mock{char q[4096];unsigned r,w,pos,phase,creates,starts,stops,destroys,steps,sends;bool running,armed;unsigned mode;};
static void push(struct mock*m,const char*s){while(*s){assert(m->w<sizeof(m->q));m->q[m->w++]=*s++;}}
static int64_t call(void*p,uint64_t op,uint64_t h,uint64_t a){
 struct mock*m=p;
 if(op==SYLE_GUEST_CREATE){assert(!h&&a==1);m->creates++;return 42;}
 assert(h==42);
 switch(op){
 case SYLE_GUEST_START:
  assert(!m->running);m->starts++;if(m->starts==2&&m->mode==3)return -7;
  m->running=true;
  if(m->starts==1)push(m,"SYLE_LINUX_USERSPACE_READY\r\n/ # ");
  else {assert(m->armed);m->phase=1;m->pos=0;if(m->mode==1)m->armed=false;}
  return 0;
 case SYLE_GUEST_STEP:assert(m->running);m->steps++;return 0;
 case SYLE_GUEST_CONSOLE_RECV:return m->r==m->w?-16:(uint8_t)m->q[m->r++];
 case SYLE_GUEST_CONSOLE_SPACE:assert(m->running);return m->mode==4?0:16;
 case SYLE_GUEST_CONSOLE_SEND:{
  assert(m->running);const char*c=m->phase?MG_RESUME_CHECK:MG_RESUME_ARM;
  assert(a==(uint8_t)c[m->pos++]);m->sends++;
  if(m->mode==5)return -7;
  if(a=='\n'){
   push(m,"\r\n");
   if(!m->phase){m->armed=true;push(m,"SYLE_RESUME_ARMED\r\n/ # ");}
   else if(m->armed&&m->mode!=2)push(m,"SYLE_RESUME_OK\r\n/ # ");
   else push(m,"/ # ");
  }else {char b[2]={(char)a,0};push(m,b);}
  return 0;}
 case SYLE_GUEST_STOP:m->running=false;m->stops++;return m->mode==6?-7:0;
 case SYLE_GUEST_DESTROY:m->destroys++;return m->mode==7?-7:0;
 default:assert(0);return -1;
 }
}
static unsigned pauses;
static bool pause_hook(void*p,uint64_t h){struct mock*m=p;assert(h==42&&!m->running&&m->creates==1&&m->starts==1&&m->stops==1);++pauses;return m->mode!=9;}
static bool output(void*p,uint8_t b){(void)b;return ((struct mock*)p)->mode!=8;}
int main(void){
 for(unsigned mode=0;mode<=8;mode++){
  struct mock m={.mode=mode};struct mg_continuous_ops ops={&m,call,output};struct mg_resume_report r;
  enum mg_resume_result result=mg_resume_run(&ops,1500,32,4,&r);
  if(mode==0){assert(result==MG_RESUME_VERIFIED&&r.armed&&r.resumed&&r.verified);assert(m.starts==2&&m.stops==2);}
  if(mode==1||mode==2||mode==4)assert(result==MG_RESUME_TIMEOUT&&!r.verified);
  if(mode==3||mode==5||mode==8)assert(result==MG_RESUME_FAILED);
  if(mode==6||mode==7)assert(result==MG_RESUME_CLEANUP_FAILED);
  assert(m.creates==1&&m.destroys==1);
 }
 for(unsigned mode=0;mode<=9;mode+=9){struct mock m={.mode=mode};struct mg_continuous_ops ops={&m,call,output};struct mg_resume_report r;pauses=0;
  enum mg_resume_result result=mg_resume_run_with_pause(&ops,1500,32,4,&r,MG_RESUME_ARM,MG_RESUME_CHECK,pause_hook);
  assert(pauses==1&&m.creates==1&&m.destroys==1);assert(mode==0?result==MG_RESUME_VERIFIED:result==MG_RESUME_FAILED);assert(m.starts==(mode==0?2u:1u));}
 struct mg_resume_stream s={0};unsigned markers=0;
 const char*echo=MG_RESUME_ARM MG_RESUME_CHECK;
 while(*echo)markers|=mr_byte(&s,(uint8_t)*echo++);
 assert(!markers);
 assert(mg_resume_run(0,1,1,1,0)==MG_RESUME_FAILED);
 puts("same guest STOP/START resume probe PASS (host mocks only)");
}
