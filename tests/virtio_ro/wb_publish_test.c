#include "wb_publish.h"
#include <assert.h>
#include <stdio.h>
int main(void){volatile unsigned before=0,after=0;before=1;vro_wb_publish();after=2;assert(before==1&&after==2);puts("x86 MFENCE primitive executes: PASS (not concurrent DMA validation)");return 0;}
