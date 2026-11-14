#include "firmware_observer_compact.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct out {char bytes[40000];size_t n;unsigned calls,fail;struct foc_trace*recursive;};
static bool sink(void*p,const char*b,size_t n){struct out*o=p;assert(n<=sizeof o->bytes-o->n);memcpy(o->bytes+o->n,b,n);o->n+=n;o->calls++;if(o->recursive)assert(!foc_flush(o->recursive));return o->calls!=o->fail;}
int main(void){uint8_t buf[20000],buf2[20000];struct foc_trace t={0},d={0};struct out a={0},b={0};
 assert(foc_init(&t,buf,sizeof buf,&a,sink)&&foc_init(&d,buf2,sizeof buf2,&b,sink)&&foc_defer(&t));
 char text[]="copied text\n";for(unsigned i=0;i<10;i++){assert(foc_text(&t,text,12)&&foc_text(&d,text,12));assert(foc_hex(&t,i)&&foc_hex(&d,i));}
 memset(text,'X',sizeof text);assert(t.dict_count==1&&t.used==134);assert(foc_flush(&t)&&a.n==b.n&&!memcmp(a.bytes,b.bytes,a.n)&&t.decoded_events==20&&t.decoded_bytes==310);
 assert(foc_flush(&t)&&a.calls==20);assert(!foc_defer(&t));
 /* Dictionary exhaustion safely falls back to owned raw literals. */
 t=(struct foc_trace){0};a=(struct out){0};assert(foc_init(&t,buf,sizeof buf,&a,sink)&&foc_defer(&t));
 for(unsigned i=0;i<514;i++){char v[4]={(char)i,(char)(i>>8),'x','\n'};assert(foc_text(&t,v,4));}
 char longtext[513];memset(longtext,'z',sizeof longtext);assert(foc_text(&t,longtext,sizeof longtext));
 assert(t.dict_count==512&&foc_flush(&t)&&a.n==514*4+513);
 /* Whole-event boundary and marker; retained prefix emits once after marker. */
 t=(struct foc_trace){0};a=(struct out){0};assert(foc_init(&t,buf,8,&a,sink)&&foc_defer(&t));assert(foc_text(&t,"abc",3));
 assert(!foc_hex(&t,0)&&t.used==8&&t.dropped_bytes==19&&t.failed);assert(!foc_flush(&t)&&a.calls==2&&!memcmp(a.bytes+a.n-3,"abc",3));assert(!foc_flush(&t)&&a.calls==2);
 for(unsigned k=0;k<3;k++){t=(struct foc_trace){0};a=(struct out){.fail=k==0?1:0};assert(foc_init(&t,buf,sizeof buf,&a,sink)&&foc_defer(&t));assert(foc_text(&t,"x",1)&&foc_hex(&t,1));
  if(k==1)a.recursive=&t;
  if(k==2){buf[0]=2;buf[1]=0;buf[2]=0;} /* forward/missing ref */
  assert(!foc_flush(&t)&&t.failed);unsigned calls=a.calls;assert(!foc_flush(&t)&&a.calls==calls);
 }
 /* Malformed lengths/tags/duplicate defs cannot read beyond encoded prefix. */
 for(unsigned k=0;k<4;k++){t=(struct foc_trace){0};a=(struct out){0};assert(foc_init(&t,buf,sizeof buf,&a,sink)&&foc_defer(&t));assert(foc_text(&t,"a",1)&&foc_text(&t,"b",1));
  if(k==0)buf[0]=99;
  if(k==1)buf[3]=255;
  if(k==2)buf[7]=0;
  if(k==3)t.used=1;
  assert(!foc_flush(&t)&&t.failed);}
 t=(struct foc_trace){0};a=(struct out){0};assert(foc_init(&t,buf,sizeof buf,&a,sink)&&foc_defer(&t));assert(!foc_text(&t,(char*)buf,1)&&t.failed&&!t.used);
 t=(struct foc_trace){0};assert(!foc_init(&t,&t,sizeof t,0,sink));assert(foc_init(&t,buf,sizeof buf,&a,sink));t.requested_bytes=UINT64_MAX;assert(!foc_hex(&t,1)&&t.failed&&t.requested_bytes==UINT64_MAX);
 puts("compact trace: PASS lossless roundtrip/bounds/faults, no heap or clocks");}
