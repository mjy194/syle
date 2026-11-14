#include "wb_publish.h"
#if !defined(__x86_64__)
#error "WB publication primitive requires reviewed x86-64 semantics"
#endif
void vro_wb_publish(void){__asm__ volatile("mfence" ::: "memory");}
