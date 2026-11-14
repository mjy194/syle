#include "../guest_terminal.h"
#include <assert.h>
#include <stdio.h>
struct mock {unsigned starts,steps,reads,sends,stops,destroys,reaps,receives,outputs;int mode;bool slot;};
static int64_t call(void*p,uint64_t op,uint64_t h,uint64_t a){
 struct mock*m=p;
 if(op==SYLE_GUEST_CREATE){assert(!h&&a==1);return (m->mode==1||m->mode==2)?-7:42;}
 if(op==SYLE_GUEST_REAP){m->reaps++;return m->mode==2?-7:0;}
 if(op==SYLE_HOST_CONSOLE_RECV){assert(!h&&!a&&m->slot);m->reads++;
  if(m->mode==16)return SYLE_HOST_CONSOLE_UNAVAILABLE;
  if(m->mode==17)return 256;
  if(m->mode==3)return SYLE_HOST_CONSOLE_LINE_ERROR;
  if(m->mode==4)return SYLE_HOST_CONSOLE_EMPTY;
  return m->reads==2?0x1d:'x';}
 assert(h==42);
 switch(op){
 case SYLE_GUEST_START:assert(!a);m->starts++;m->slot=false;return m->mode==18?-7:0;
 case SYLE_GUEST_STEP:assert(!a);m->steps++;m->slot=false;return m->mode==5?-7:0;
 case SYLE_GUEST_CONSOLE_RECV:assert(!a);m->receives++;if(m->mode==6)return -7;
  {static const char boot[]="SYLE_LINUX_USERSPACE_READY\n# ";
   if(m->mode!=13&&m->receives<sizeof(boot))return boot[m->receives-1];}
  return -16;
 case SYLE_GUEST_CONSOLE_SPACE:assert(!a);if(m->mode==7)return 0;if(m->mode==8)return -7;if(m->mode==9)return 17;if(m->mode==14)return -SYLE_GUEST_EBUSY;m->slot=true;return 1;
 case SYLE_GUEST_CONSOLE_SEND:assert(m->slot&&a=='x');m->slot=false;m->sends++;return m->mode==10?-16:0;
 case SYLE_GUEST_STOP:assert(!a);m->stops++;return m->mode==11?-7:0;
 case SYLE_GUEST_DESTROY:assert(!a);m->destroys++;return m->mode==12?-7:0;
 default:assert(0);return -1;
 }
}
static bool output(void*p,uint8_t b){(void)b;struct mock*m=p;m->outputs++;return m->mode!=15;}
int main(void){
 for(int mode=0;mode<=18;mode++){
  struct mock m={.mode=mode};struct mg_continuous_ops o={&m,call,output};struct mg_terminal_report report;
  enum mg_terminal_result r=mg_terminal_run(&o,3,32,&report);
  enum mg_terminal_result want=mode==0?MG_TERMINAL_DETACHED:(mode==4||mode==7||mode==13||mode==14)?MG_TERMINAL_BUDGET:(mode==2||mode==11||mode==12)?MG_TERMINAL_CLEANUP_FAILED:MG_TERMINAL_FAILED;
  assert(r==want);
  if(mode==1||mode==2)assert(m.reaps==1&&!m.stops&&!m.destroys);
  else assert(m.stops==1&&m.destroys==1);
  if(mode==0)assert(report.sent==1&&m.sends==1&&m.reads==2&&m.starts==1&&m.steps==1);
  if(mode==7||mode==13||mode==14)assert(!m.reads&&!m.sends&&m.steps==2);
  if(mode==10)assert(m.sends==1&&m.reads==1&&!report.sent);
 }
 assert(mg_terminal_run(0,1,1,0)==MG_TERMINAL_FAILED);
 puts("terminal input/space flow and cleanup PASS (host mocks only)");
}
