#include "../guest_shell_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct mock {char q[512];unsigned r,w,sent,stop,destroy,reap,steps;int mode;bool outstanding;};
static void push(struct mock*m,const char*s){while(*s)m->q[m->w++]=*s++;assert(m->w<sizeof(m->q));}
static int64_t call(void*p,uint64_t op,uint64_t h,uint64_t a){
 struct mock*m=p;static const char cmd[]=MG_SHELL_COMMAND;
 if(op==SYLE_GUEST_CREATE){assert(!h&&a==1);return m->mode==7?-7:42;}
 if(op==SYLE_GUEST_REAP){m->reap++;return 0;}
 assert(h==42);
 switch(op){
 case SYLE_GUEST_START:
  assert(!a);if(m->mode!=1)push(m,"boot\r\n" MG_SHELL_READY "\r\n/ # ");return 0;
 case SYLE_GUEST_STEP:
  assert(!a);m->steps++;return m->mode==9&&m->sent==sizeof(cmd)-1?-7:0;
 case SYLE_GUEST_CONSOLE_RECV:
  assert(!a);if(m->mode==8)return -6;
  if(m->r==m->w)return -16;
  m->outstanding=false;return (uint8_t)m->q[m->r++];
 case SYLE_GUEST_CONSOLE_SEND:
  assert(m->sent<sizeof(cmd)-1&&!m->outstanding&&a==(uint8_t)cmd[m->sent]);
  m->sent++;m->outstanding=true;if(m->mode==4)return -16;
  if(a=='\n'){
   push(m,"\r\n");if(m->mode==0||m->mode==5||m->mode==6||m->mode==9)push(m,MG_SHELL_REPLY "\r\n/ # ");
   if(m->mode==3)push(m,"prefix" MG_SHELL_REPLY "\r\n");
  }else if(m->mode==10)push(m,"clocksource: message\n");
  else if(m->mode!=2){char s[2]={(char)a,0};push(m,s);if(m->mode==11)push(m,"clocksource: message\n");}
  return 0;
 case SYLE_GUEST_STOP:m->stop++;return m->mode==5?-7:0;
 case SYLE_GUEST_DESTROY:m->destroy++;return m->mode==6?-7:0;
 default:assert(0);return -1;
 }
}
static bool out(void*p,uint8_t b){(void)p;(void)b;return true;}
struct observation {unsigned n;bool fail,ready,prompt,first,reply;};
static bool observe(void *p,const struct mg_shell_report *r){
 struct observation *o=p;o->n++;o->ready|=r->ready;o->prompt|=r->prompt;
 o->first|=r->sent==1;o->reply|=r->reply;return !o->fail;
}
int main(void){
 for(int mode=0;mode<=11;mode++){
  struct mock m={.mode=mode};struct mg_continuous_ops o={&m,call,out};
  struct mg_shell_report report;
  enum mg_shell_result r=mg_shell_probe_run_report(&o,1000,8,4,&report);
  if(mode==0)assert(report.ready&&report.prompt&&report.reply&&report.sent==sizeof(MG_SHELL_COMMAND)-1);
  if(mode==1)assert(!report.ready&&!report.prompt&&!report.sent&&!report.reply);
  if(mode==2)assert(report.sent==1&&report.echo_wait&&!report.reply);
  if(mode==4)assert(!report.sent&&report.send_result==-16);
  if(mode==9)assert(report.guest_result==-7);
  if(mode==10||mode==11)assert(report.echo_mismatch&&report.sent==1&&m.sent==1&&!report.reply);
  assert(r==(mode==0?MG_SHELL_VERIFIED:(mode==5||mode==6)?MG_SHELL_CLEANUP_FAILED:(mode==4||mode==7||mode==8||mode==9||mode==10||mode==11)?MG_SHELL_FAILED:MG_SHELL_TIMEOUT));
  if(mode==7)assert(m.reap==1&&!m.stop&&!m.destroy);
  else assert(m.stop==1&&m.destroy==1);
  if(mode==1)assert(!m.sent);
  if(mode==2||mode==4)assert(m.sent==1);
 }
 for(unsigned fail=0;fail<2;fail++) {
  struct mock m={0};struct mg_continuous_ops o={&m,call,out};
  struct observation obs={.fail=fail};struct mg_shell_report report;
  enum mg_shell_result r=mg_shell_probe_run_observed(&o,1000,8,4,&report,observe,&obs);
  assert(r==(fail?MG_SHELL_FAILED:MG_SHELL_VERIFIED));
  assert(obs.n&&obs.n<=5&&m.stop==1&&m.destroy==1);
  if(!fail)assert(obs.ready&&obs.prompt&&obs.first&&obs.reply);
 }
 struct mg_shell_stream s={0};bool matched=false;
 const char *fake="echo SYLE_\"\"SHELL_OK\r\n";
 while(*fake)matched|=mg_shell_byte(&s,(uint8_t)*fake++,true);
 assert(!matched);
 fake=MG_SHELL_REPLY "\r\n";while(*fake)matched|=mg_shell_byte(&s,(uint8_t)*fake++,false);
 assert(!matched);
 assert(mg_shell_probe_run(0,1,1,1)==MG_SHELL_FAILED);
 puts("shell fixed-command probe PASS (host mocks, not Linux acceptance)");
}
