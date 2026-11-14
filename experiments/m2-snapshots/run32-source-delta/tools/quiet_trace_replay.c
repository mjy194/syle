#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../krnl/firmware_observer_trace.h"
struct out {char*p;size_t n,cap;};
static bool sink(void*c,const char*p,size_t n){struct out*o=c;assert(n<=o->cap-o->n);memcpy(o->p+o->n,p,n);o->n+=n;return true;}
int main(int ac,char**av){assert(ac==2);FILE*f=fopen(av[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);char*src=malloc(n),*buf=malloc(FOT_MAX_CAPACITY),*dst=malloc(n);assert(fread(src,1,n,f)==n);fclose(f);struct out o={dst,0,n};struct fot_trace t={0};assert(fot_init(&t,buf,FOT_MAX_CAPACITY,&o,sink));assert(fot_defer(&t));size_t p=0;while(p<n){size_t e=p;while(e<n&&src[e]!='\n')e++;if(e<n)e++;assert(fot_text(&t,src+p,e-p));p=e;}assert(o.n==0&&!t.incomplete);assert(fot_flush(&t));assert(o.n==n&&!memcmp(src,dst,n));assert(fot_flush(&t)&&o.n==n);printf("actual run17 serial replay byte/order PASS %zu bytes\n",n);free(src);free(buf);free(dst);}
