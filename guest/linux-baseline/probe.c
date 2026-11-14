/* Linux x86-64 syscall-only program: no libc or dynamic loader dependency. */
static long call3(long n,long a,long b,long c) {
 long r; __asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a),"S"(b),"d"(c):"rcx","r11","memory"); return r;
}
static unsigned len(const char *s){unsigned n=0;while(s[n])n++;return n;}
static int eq(const char *a,const char *b){while(*a&&*a==*b)a++,b++;return *a==*b;}
static void out(const char *s){(void)call3(1,1,(long)s,len(s));}
#include "clock-probe.h"
int probe_main(long argc,char **argv){
 if(argc==2){int clock_result=cp_run(argv[1]);if(clock_result>=0)return clock_result;}
 if(argc==2&&eq(argv[1],"exit37")){out("SYLE_PROGRAM_EXIT37\n");return 37;}
 if(argc!=2||!eq(argv[1],"check")){out("usage: baseline-probe check|exit37\n");return 2;}
 const char *path="/tmp/syle-program-test"; const char *data="SYLE_FILE_ROUNDTRIP\n"; char buf[32];
 /* O_EXCL avoids following or overwriting a preexisting path. */
 long fd=call3(2,(long)path,1|64|128,0600);
 if(fd<0)return 10;
 if(call3(1,fd,(long)data,len(data))!=(long)len(data)){call3(3,fd,0,0);call3(87,(long)path,0,0);return 11;}
 if(call3(3,fd,0,0)<0)return 12;
 fd=call3(2,(long)path,0,0);if(fd<0)return 13;
 long n=call3(0,fd,(long)buf,sizeof(buf));call3(3,fd,0,0);call3(87,(long)path,0,0);
 if(n!=(long)len(data))return 14;
 for(long i=0;i<n;i++)if(buf[i]!=data[i])return 15;
 out("SYLE_PROGRAM_OK\n");return 0;
}
