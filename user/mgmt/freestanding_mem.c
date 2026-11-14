#include <stddef.h>
/* Freestanding C compilers may emit these for aggregate initialization/copies.
 * Volatile byte accesses prevent this implementation being folded into itself. */
void *memset(void *destination,int value,size_t n){
 volatile unsigned char *d=destination;
 for(size_t i=0;i<n;i++)d[i]=(unsigned char)value;
 return destination;
}
void *memcpy(void *destination,const void *source,size_t n){
 volatile unsigned char *d=destination;const volatile unsigned char *s=source;
 for(size_t i=0;i<n;i++)d[i]=s[i];
 return destination;
}
