#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware_lifecycle.h"
struct model {
 unsigned generation,state,creates,destroys,stopped_status,stopped_steps,resumes,reaps;
 uint64_t handle;bool persistent,variable,pid;
 unsigned fault;char input[512],output[2048];unsigned in,out,end;
};
static void queue(struct model*m,const char*s){assert(m->end+strlen(s)<sizeof m->output);strcpy(m->output+m->end,s);m->end+=(unsigned)strlen(s);}
static int64_t call(void*x,uint64_t op,uint64_t h,uint64_t arg){
 struct model*m=x;
 if(op==SYLE_GUEST_REAP){m->reaps++;return 0;}
 if(op==SYLE_GUEST_CREATE){
  m->creates++;if(m->fault==1)return -SYLE_GUEST_EBACKEND;
  assert(!m->state);m->generation++;m->handle=m->generation+30;m->state=SYLE_GUEST_READY;m->variable=m->pid=false;m->in=m->out=m->end=0;return m->handle;
 }
 if(h!=m->handle||!m->state)return -SYLE_GUEST_ENOENT;
 if(op==SYLE_GUEST_STATUS){assert(m->state==SYLE_GUEST_STOPPED);m->stopped_status++;return m->state;}
 if(op==SYLE_GUEST_START){
  if(m->state==SYLE_GUEST_STOPPED){m->resumes++;if(m->fault==2)m->variable=false;}
  else{assert(m->state==SYLE_GUEST_READY);queue(m,"SYLE_DISK_ROOT_READY\n# ");}
  m->state=SYLE_GUEST_RUNNING;return 0;
 }
 if(op==SYLE_GUEST_STEP){if(m->state==SYLE_GUEST_STOPPED){m->stopped_steps++;return m->fault==3?0:-SYLE_GUEST_ESTATE;}assert(m->state==SYLE_GUEST_RUNNING);return 0;}
 if(op==SYLE_GUEST_STOP){m->state=SYLE_GUEST_STOPPED;return 0;}
 if(op==SYLE_GUEST_DESTROY){m->destroys++;if(m->fault==4)return -SYLE_GUEST_EBACKEND;m->state=0;return 0;}
 if(op==SYLE_GUEST_CONSOLE_SPACE)return 16;
 if(op==SYLE_GUEST_CONSOLE_RECV){if(m->out==m->end){m->out=m->end=0;return -SYLE_GUEST_EBUSY;}return (uint8_t)m->output[m->out++];}
 if(op==SYLE_GUEST_CONSOLE_SEND){
  assert(m->in+1<sizeof m->input);m->input[m->in++]=(char)arg;m->input[m->in]=0;
  if(arg!='\n')return 0;
  queue(m,m->input);
  if(strstr(m->input,"persistctl write")){m->persistent=m->variable=m->pid=true;queue(m,"SYLE_LIFE_ARMED\n# ");}
  else if(strstr(m->input,"m2pid")){if(m->variable&&m->pid&&m->persistent)queue(m,"SYLE_LIFE_RESUMED\n# ");}
  else if(m->persistent&&m->fault!=5)queue(m,"SYLE_LIFE_RECREATED\n# ");
  m->in=0;return 0;
 }
 assert(0);return -1;
}
static bool output(void*x,uint8_t b){(void)b;return ((struct model*)x)->fault!=6;}
int main(void){
 struct fml_stream s={0};const char*echo="# echo SYLE_\"\"LIFE_ARMED\n";while(*echo)assert(!fml_byte(&s,(uint8_t)*echo++));
 for(unsigned i=0;i<300;i++)assert(!fml_byte(&s,'x'));
 assert(!fml_byte(&s,'\n'));
 const char*marker="SYLE_LIFE_ARMED\r\n";unsigned seen=0;while(*marker)seen|=fml_byte(&s,(uint8_t)*marker++);assert(seen==1);
 for(unsigned fault=0;fault<=6;fault++){
  struct model m={.fault=fault};const struct mg_continuous_ops o={&m,call,output};int r=fml_run(&o);
  assert((r==0)==(fault==0));
  if(!fault){assert(m.creates==2&&m.destroys==2&&m.resumes==1&&m.stopped_status==256&&m.stopped_steps==1&&!m.state&&m.persistent);}
  else if(fault==1)assert(m.reaps==1&&!m.destroys);
  else assert(m.destroys>=1);
 }
 puts("firmware lifecycle: exact response, pause/state retention, stopped STEP, stale handles, durable recreate and failure cleanup PASS");
}
