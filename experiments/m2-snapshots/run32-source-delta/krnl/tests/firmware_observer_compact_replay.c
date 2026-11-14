#include "firmware_observer_compact.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct output {const unsigned char *p;size_t n,pos;};
static bool sink(void*c,const char*p,size_t n){struct output*o=c;assert(n<=o->n-o->pos);assert(!memcmp(o->p+o->pos,p,n));o->pos+=n;return true;}
static unsigned char*load(const char*p,size_t*n){FILE*f=fopen(p,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long z=ftell(f);assert(z>=0);*n=(size_t)z;rewind(f);unsigned char*b=malloc(*n?*n:1);assert(b);assert(fread(b,1,*n,f)==*n);fclose(f);return b;}
static unsigned long long le(const unsigned char*p,unsigned n){unsigned long long v=0;for(unsigned i=0;i<n;i++)v|=(unsigned long long)p[i]<<(8*i);return v;}
int main(int argc,char**argv){assert(argc==3);size_t n,m;unsigned char*tape=load(argv[1],&n),*expected=load(argv[2],&m),*buf=malloc(FOC_MAX_CAPACITY);assert(buf);struct foc_trace t={0};struct output o={expected,m,0};assert(foc_init(&t,buf,FOC_MAX_CAPACITY,&o,sink));assert(foc_defer(&t));size_t off=0;while(off<n){unsigned char tag=tape[off++];if(tag=='T'){assert(n-off>=4);size_t z=le(tape+off,4);off+=4;assert(z<=n-off);assert(foc_text(&t,(char*)tape+off,z));off+=z;}else{assert(tag=='H'&&n-off>=8);assert(foc_hex(&t,le(tape+off,8)));off+=8;}}assert(foc_flush(&t));assert(o.pos==m&&t.accepted_bytes==m&&t.decoded_bytes==m);assert(foc_flush(&t)&&o.pos==m);printf("lossless PASS decoded=%zu encoded=%zu dictionary=%u events=%llu ratio=%.3f\n",m,t.used,t.dict_count,(unsigned long long)t.accepted_events,(double)t.used/m);free(tape);free(expected);free(buf);}
