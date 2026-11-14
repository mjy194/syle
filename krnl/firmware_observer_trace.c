#include "firmware_observer_trace.h"
static bool poison(struct fot_trace*t){if(t){t->failed=true;t->incomplete=true;}return false;}
static bool add(struct fot_trace*t,uint64_t*n,uint64_t v){
 if(v>UINT64_MAX-*n){*n=UINT64_MAX;return poison(t);}*n+=v;return true;
}
static bool emit(struct fot_trace*t,const char*p,size_t n){
 if(t->sink_failed||t->busy)return poison(t);
 if(!add(t,&t->sink_attempted_bytes,n)){t->sink_failed=true;return false;}
 t->busy=true;bool ok=t->sink(t->context,p,n);t->busy=false;
 if(!ok||t->sink_failed){t->sink_failed=true;return poison(t);}return true;
}
bool fot_init(struct fot_trace*t,char*b,size_t cap,void*c,bool(*sink)(void*,const char*,size_t)){
 if(!t||t->ready||t->failed||!b||!cap||cap>FOT_MAX_CAPACITY||!sink)return false;
 *t=(struct fot_trace){.buffer=b,.capacity=cap,.context=c,.sink=sink,.ready=true};return true;
}
bool fot_defer(struct fot_trace*t){
 if(!t||!t->ready)return false;
 if(t->busy){t->sink_failed=true;return poison(t);}
 if(t->failed||t->deferred||t->flushed)return false;
 t->deferred=true;return true;
}
static bool record(struct fot_trace*t,const char*p,size_t n,bool hex){
 if(!t||!t->ready)return false;
 if(t->busy){t->sink_failed=true;return poison(t);}
 bool count=add(t,hex?&t->hex_calls:&t->text_calls,1);
 count=add(t,&t->requested_bytes,n)&&count;
 if(!p&&n){add(t,&t->dropped_bytes,n);return poison(t);}
 if(t->failed||!count){add(t,&t->dropped_bytes,n);return false;}
 if(!n)return true;
 if(!t->deferred)return emit(t,p,n);
 if(n>t->capacity-t->used){
  add(t,&t->dropped_bytes,n);poison(t);
  if(!t->marker_attempted){t->marker_attempted=true;
   (void)emit(t,FOT_OVERFLOW_MARKER,sizeof(FOT_OVERFLOW_MARKER)-1);}
  return false;
 }
 for(size_t i=0;i<n;i++)t->buffer[t->used+i]=p[i];
 t->used+=n;(void)add(t,&t->buffered_bytes,n);return true;
}
bool fot_text(struct fot_trace*t,const char*p,size_t n){return record(t,p,n,false);}
bool fot_hex(struct fot_trace*t,uint64_t v){
 const char digits[]="0123456789abcdef";char b[19];b[0]='0';b[1]='x';
 for(unsigned i=0;i<16;i++)b[2+i]=digits[(v>>(60-i*4))&15];
 b[18]='\n';
 return record(t,b,sizeof b,true);
}
bool fot_flush(struct fot_trace*t){
 if(!t||!t->ready)return false;
 if(t->busy){t->sink_failed=true;return poison(t);}
 if(t->flushed)return !t->failed;
 t->flushed=true;t->deferred=false;
 if(t->used&&!t->sink_failed){size_t n=t->used;
  /* used retained for diagnostics; flushed prevents all repeat delivery. */
  (void)emit(t,t->buffer,n);
 }
 return !t->failed;
}
