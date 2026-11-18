#include "../firmware_boot_invlpg.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint64_t r[16]={0};r[7]=0xffffffffff400000;uint8_t b[]={0x0f,1,0x3f};assert(fbinv_decode(b,3,3,r,r[7]));assert(!fbinv_decode(b,3,3,r,r[7]+1));b[2]=0xff;assert(!fbinv_decode(b,3,3,r,r[7]));uint8_t x[]={0x41,0x0f,1,0x38};r[8]=0x1000;assert(fbinv_decode(x,4,4,r,0x1000));assert(!fbinv_decode(x,3,4,r,0x1000));r[8]=0x800000000000;assert(!fbinv_decode(x,4,4,r,r[8]));puts("INVLPG owned instruction address decoding PASS");}
