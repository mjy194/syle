/* Freestanding Linux framebuffer acceptance. Never run on the development host. */
#include <linux/fb.h>
static long call(long n,long a,long b,long c,long d){long r;register long r10 __asm__("r10")=d;register long r8 __asm__("r8")=0;__asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a),"S"(b),"d"(c),"r"(r10),"r"(r8):"rcx","r11","memory");return r;}
static void say(const char*s){long n=0;while(s[n])n++;call(1,1,(long)s,n,0);}
static int run(void){
 long fd=call(257,-100,(long)"/dev/fb0",2,0);struct fb_var_screeninfo v={0};struct fb_fix_screeninfo f={0};
 if(fd<0||call(16,fd,FBIOGET_VSCREENINFO,(long)&v,0)||call(16,fd,FBIOGET_FSCREENINFO,(long)&f,0))return 1;
 if(v.xres!=800||v.yres!=600||v.bits_per_pixel!=32||v.red.offset!=16||v.green.offset!=8||v.blue.offset!=0||f.line_length<3200||f.smem_len<(unsigned long)f.line_length*600)return 1;
 unsigned row[1024];if(f.line_length>sizeof row)return 1;
 for(unsigned y=0;y<600;y++){
  for(unsigned x=0;x<f.line_length/4;x++)row[x]=y<300?(x<400?0x00ffff:0xff00ff):(x<400?0xffff00:0x202020);
  if(call(1,fd,(long)row,f.line_length,0)!=(long)f.line_length)return 1;
 }
 if(call(3,fd,0,0,0))return 1;
 say("SYLE_FB_PATTERN_READY 800x600 BGRX\n");long pause[2]={2,0};if(call(35,(long)pause,0,0,0))return 1;
 say("SYLE_FB_HANDOFF_VERIFIED\n");return 0;
}
void start_c(void){int r=run();if(r)say("SYLE_FB_ERROR\n");call(60,r,0,0,0);for(;;){}}
__asm__(".global _start\n_start:\nand $-16,%rsp\ncall start_c\nud2\n");
