/* Compiler lowering support, linked only into opt-in recovery image. */
#include <stddef.h>
void *memset(void *d,int c,size_t n){volatile unsigned char*p=d;while(n--)*p++=(unsigned char)c;return d;}
void *memcpy(void *d,const void *s,size_t n){volatile unsigned char*p=d;const volatile unsigned char*q=s;while(n--)*p++=*q++;return d;}
