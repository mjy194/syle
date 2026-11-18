/* Freestanding Linux userspace acceptance helper, installed on the ext2 root.
 * It never supplies a block driver or copies an initramfs into a fake root. */
typedef unsigned long usize;
static long call(long n,long a,long b,long c,long d){long r;register long r8 __asm__("r8")=0;register long r10 __asm__("r10")=d;__asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a),"S"(b),"d"(c),"r"(r10),"r"(r8):"rcx","r11","memory");return r;}
static usize len(const char*s){usize n=0;while(s[n])n++;return n;}
static int equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void say(const char*s){call(1,1,(long)s,len(s),0);}
static int fail(const char*s){say("SYLE_PERSIST_ERROR ");say(s);say("\n");return 1;}
static int run(long argc,char**argv){
 if(argc<2)return fail("arguments");
 long fs[32];if(call(137,(long)"/",(long)fs,0,0)||fs[0]!=0xef53)return fail("root_not_ext2");
 if(equal(argv[1],"reboot")){call(162,0,0,0,0);if(call(165,0,(long)"/",0,33))return fail("remount_readonly");say("SYLE_PERSIST_ROOT_REMOUNTED_RO\nSYLE_PERSIST_REBOOT_REQUEST\n");call(169,0xfee1dead,0x28121969,0x01234567,0);return fail("reboot_returned");}
 if(argc!=3)return fail("token_missing");
 usize n=len(argv[2]);if(n<16||n>96)return fail("token_length");
 char b[128];for(usize i=0;i<n;i++){char c=argv[2][i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return fail("token_character");b[i]=c;}b[n]='\n';
 if(equal(argv[1],"write")){
  long fd=call(257,-100,(long)"/persist/probe.txt",0x241,0600);if(fd<0)return fail("open_write");
  if(call(1,fd,(long)b,n+1,0)!=(long)n+1||call(74,fd,0,0,0)||call(3,fd,0,0,0))return fail("file_fsync");
  long dir=call(257,-100,(long)"/persist",0x10000,0);if(dir<0||call(74,dir,0,0,0)||call(3,dir,0,0,0))return fail("directory_fsync");
  say("SYLE_PERSIST_WRITTEN ");say(argv[2]);say("\n");return 0;
 }
 if(equal(argv[1],"read")){
  char got[128];long fd=call(257,-100,(long)"/persist/probe.txt",0,0);if(fd<0)return fail("open_read");
  long r=call(0,fd,(long)got,sizeof got,0);if(call(3,fd,0,0,0)||r!=(long)n+1)return fail("length");
  for(usize i=0;i<n+1;i++)if(got[i]!=b[i])return fail("content");
  say("SYLE_PERSIST_VERIFIED ");say(argv[2]);say("\n");return 0;
 }
 return fail("unknown_operation");
}
void start_c(long*stack){int code=run(stack[0],(char**)(stack+1));call(60,code,0,0,0);for(;;){};}
__asm__(".global _start\n_start:\nmov %rsp,%rdi\nand $-16,%rsp\ncall start_c\nud2\n");
