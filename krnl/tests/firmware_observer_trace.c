#include "../firmware_observer_trace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct output {char bytes[1024];size_t n;unsigned calls,fail;struct fot_trace*reenter;};
static bool sink(void*p,const char*b,size_t n){struct output*o=p;o->calls++;assert(n<=sizeof o->bytes-o->n);
 memcpy(o->bytes+o->n,b,n);o->n+=n;
 if(o->reenter)assert(!fot_text(o->reenter,"recursive",9));
 return o->calls!=o->fail;
}
static void sequence(struct fot_trace*t){assert(fot_text(t,"hello\n",6));assert(fot_hex(t,0));assert(fot_hex(t,UINT64_MAX));assert(fot_hex(t,0x123456789abcdef));assert(fot_text(t,"bye",3));}
int main(void){char a[128],b[128];struct fot_trace direct={0},defer={0};struct output x={0},y={0};
 assert(fot_init(&direct,a,sizeof a,&x,sink)&&fot_init(&defer,b,sizeof b,&y,sink));assert(fot_defer(&defer));
 sequence(&direct);sequence(&defer);assert(!y.calls&&defer.used==66&&defer.buffered_bytes==66);
 assert(fot_flush(&defer)&&y.calls==1&&x.n==y.n&&!memcmp(x.bytes,y.bytes,x.n));
 assert(!memcmp(x.bytes,"hello\n0x0000000000000000\n0xffffffffffffffff\n0x0123456789abcdef\nbye",66));
 assert(defer.text_calls==2&&defer.hex_calls==3&&defer.requested_bytes==66&&!defer.dropped_bytes);
 assert(fot_flush(&defer)&&y.calls==1);assert(fot_text(&defer,"!",1)&&y.calls==2);assert(!fot_defer(&defer));
 /* Exact capacity then overflow: whole event dropped, prefix once after marker. */
 struct fot_trace t={0};struct output o={0};assert(fot_init(&t,a,3,&o,sink)&&fot_defer(&t));
 assert(fot_text(&t,"abc",3));assert(!fot_text(&t,"def",3)&&t.failed&&t.incomplete&&t.used==3);
 assert(o.calls==1&&o.n==sizeof(FOT_OVERFLOW_MARKER)-1);
 assert(!fot_hex(&t,1)&&t.dropped_bytes==22&&t.requested_bytes==25&&t.hex_calls==1&&t.text_calls==2);
 assert(!fot_flush(&t)&&o.calls==2&&!memcmp(o.bytes+sizeof(FOT_OVERFLOW_MARKER)-1,"abc",3));
 assert(!fot_flush(&t)&&o.calls==2&&t.sink_attempted_bytes==o.n);
 /* Direct and drain sink failures are uncertain prefix, never retried. */
 for(unsigned k=0;k<2;k++){t=(struct fot_trace){0};o=(struct output){.fail=1};assert(fot_init(&t,a,128,&o,sink));
  if(k){assert(fot_defer(&t));assert(fot_text(&t,"abc",3));assert(!fot_flush(&t));}
  else assert(!fot_text(&t,"abc",3));
  assert(t.sink_failed&&t.failed&&t.incomplete&&o.calls==1);assert(!fot_flush(&t)&&o.calls==1);
  assert(!fot_text(&t,"drop",4)&&t.dropped_bytes==4);
 }
 /* Marker failure cannot safely be followed by prefix drain. */
 t=(struct fot_trace){0};o=(struct output){.fail=1};assert(fot_init(&t,a,1,&o,sink)&&fot_defer(&t));
 assert(fot_text(&t,"x",1));assert(!fot_text(&t,"yy",2));assert(!fot_flush(&t)&&o.calls==1&&t.used==1);
 /* Reentry latches failure, does not append or invoke sink recursively. */
 t=(struct fot_trace){0};o=(struct output){.reenter=&t};assert(fot_init(&t,a,128,&o,sink));
 assert(!fot_text(&t,"a",1)&&t.sink_failed&&o.calls==1);assert(!fot_flush(&t)&&o.calls==1);
 /* Explicit bounds/null/empty, counter overflow saturates and stops. */
 t=(struct fot_trace){0};o=(struct output){0};assert(!fot_init(&t,a,FOT_MAX_CAPACITY+1,&o,sink));
 assert(fot_init(&t,a,128,&o,sink));assert(fot_text(&t,0,0)&&!o.calls);
 t.requested_bytes=UINT64_MAX;assert(!fot_text(&t,"x",1)&&t.requested_bytes==UINT64_MAX&&t.failed&&!o.calls);
 assert(!fot_init(&t,a,128,&o,sink));assert(!fot_flush(0));
 puts("firmware_observer_trace: PASS (no clock reads; at-most-once sink prefix)");
}
