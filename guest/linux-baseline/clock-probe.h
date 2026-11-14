#ifndef SYLE_BASELINE_CLOCK_PROBE_H
#define SYLE_BASELINE_CLOCK_PROBE_H
/* Uses existing syscall/output helpers. No vDSO or libc clocks. */
struct cp_timespec {long sec,nsec;};
struct cp_sample {unsigned long mono,raw,tsc;};
static int cp_ns(long clock,unsigned long *value){
 struct cp_timespec t;
 if(call3(228,clock,(long)&t,0)||t.sec<0||t.nsec<0||t.nsec>=1000000000L||
    (unsigned long)t.sec>(~0ul-(unsigned long)t.nsec)/1000000000ul)return 0;
 *value=(unsigned long)t.sec*1000000000ul+(unsigned long)t.nsec;return 1;
}
static int cp_sample(struct cp_sample*s){
 unsigned lo,hi;
 if(!cp_ns(1,&s->mono)||!cp_ns(4,&s->raw))return 0;
 __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi)::"memory");
 s->tsc=((unsigned long)hi<<32)|lo;return 1;
}
static void cp_number(const char*name,unsigned long value){
 char b[32];unsigned n=0;do{b[n++]=(char)('0'+value%10);value/=10;}while(value);
 out(name);while(n){char c=b[--n];(void)call3(1,1,(long)&c,1);}out("\n");
}
static int cp_run(const char*mode){
 unsigned ms=0;int spin=0,pause=0;long pause_pid=0;
 if(eq(mode,"clock0"))ms=0;else if(eq(mode,"clock10"))ms=10;
 else if(eq(mode,"clock100"))ms=100;else if(eq(mode,"clock1000"))ms=1000;
 else if(eq(mode,"clockspin"))spin=1;else if(eq(mode,"clockpause"))pause=1;else return -1;
 struct cp_sample before,after;
 out("\nSYLE_CLOCK_BEGIN ");out(mode);out("\n");
 if(!cp_sample(&before)){out("SYLE_CLOCK_ERROR sample-before\n");return 20;}
 if(pause){
  pause_pid=call3(39,0,0,0);if(pause_pid<=0)return 24;
  cp_number("pause_pid=",(unsigned long)pause_pid);
  out("\nSYLE_RESUME_ARMED\n");char byte=0;
  if(call3(0,0,(long)&byte,1)!=1||byte!='\n'||call3(39,0,0,0)!=pause_pid){out("SYLE_CLOCK_ERROR pause-read\n");return 25;}
 }else if(spin){volatile unsigned long v=1;for(unsigned i=0;i<1000000;i++)v=v*33+i;}
 else {struct cp_timespec req={(long)(ms/1000),(long)(ms%1000)*1000000};
  if(call3(35,(long)&req,0,0)){out("SYLE_CLOCK_ERROR nanosleep\n");return 21;}}
 if(!cp_sample(&after)){out("SYLE_CLOCK_ERROR sample-after\n");return 22;}
 if(after.mono<before.mono||after.raw<before.raw||after.tsc<before.tsc){out("SYLE_CLOCK_ERROR backwards\n");return 23;}
 out("SYLE_CLOCK_END ");out(mode);out("\n");
 cp_number("mono_ns=",after.mono-before.mono);
 cp_number("raw_ns=",after.raw-before.raw);
 cp_number("tsc_delta=",after.tsc-before.tsc);
 out("SYLE_CLOCK_DONE ");out(mode);out("\n");if(pause){cp_number("pause_pid_after=",(unsigned long)pause_pid);out("SYLE_RESUME_OK\n");}return 0;
}
#endif
