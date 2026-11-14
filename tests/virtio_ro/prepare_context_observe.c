#include <stdint.h>
#ifdef PREPARE_OBSERVE_HOST_TEST
extern unsigned char observe_test_in(uint16_t);
extern void observe_test_out(uint16_t,unsigned char);
#define input observe_test_in
#define output observe_test_out
#else
static unsigned char input(uint16_t port){unsigned char b;__asm__ volatile("inb %w1,%0":"=a"(b):"Nd"(port));return b;}
static void output(uint16_t port,unsigned char b){__asm__ volatile("outb %0,%w1"::"a"(b),"Nd"(port));}
#endif
extern void source_prepare_pause(void);
void source_prepare_observe(void){
 static const char marker[]="SOURCE.PREPARE_CONTEXT_WINDOW\r\n";
 for(unsigned j=0;j<sizeof marker-1;j++){
  unsigned i;for(i=0;i<10000;i++)if(input(0x3fd)&0x20)break;
  if(i==10000)return; /* still returns only to unconditional prepare failure */
  output(0x3f8,(unsigned char)marker[j]);
 }
 source_prepare_pause();
}
