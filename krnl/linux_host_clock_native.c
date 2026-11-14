#include "linux_host_clock_hv.h"
extern bool lvn_read_clock_reference(uint64_t*);
extern void log_text(const char*);
extern void log_hex(uint64_t);
static struct lhch_context context;
static bool cpuid(void*p,uint32_t leaf,uint32_t r[4]){(void)p;__asm__ volatile("cpuid":"=a"(r[0]),"=b"(r[1]),"=c"(r[2]),"=d"(r[3]):"a"(leaf),"c"(0));return true;}
static bool reference(void*p,uint64_t*v){(void)p;return lvn_read_clock_reference(v);}
bool linux_host_clock_native_source(struct lhcr_source*out){
 const struct lhch_ops ops={0,cpuid,reference};
 if(!lhch_admit(&context,&ops,1,out)){log_text("HOST_CLOCK Hyper-V reference counter admission rejected\n");return false;}
 log_text("HOST_CLOCK experimental Hv#1 reference-counter hz=");log_hex(out->properties.hz);
 log_text(" cpu=");log_hex(out->properties.cpu);log_text(" max_gap=");log_hex(out->max_gap);log_text("\n");return true;
}
