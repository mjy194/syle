#include "firmware_observer_compact.h"
enum {DEF=1,REF=2,RAW=3,HEX=4};
static bool fail(struct foc_trace*t){if(t){t->failed=true;t->incomplete=true;}return false;}
static bool add(struct foc_trace*t,uint64_t*n,uint64_t v){if(v>UINT64_MAX-*n){*n=UINT64_MAX;return fail(t);}*n+=v;return true;}
static bool overlap(const void*a,size_t n,const void*b,size_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(n>UINTPTR_MAX-x||m>UINTPTR_MAX-y)return true;
 return n&&m&&x<y+m&&y<x+n;
}
static bool enter(struct foc_trace*t){if(!t||!t->ready)return false;if(t->busy){t->sink_failed=true;return fail(t);}return true;}
static bool emit(struct foc_trace*t,const uint8_t*p,size_t n){
 if(t->sink_failed||t->busy)return fail(t);
 if(!add(t,&t->sink_attempted_bytes,n)){t->sink_failed=true;return false;}
 t->busy=true;bool ok=t->sink(t->context,(const char*)p,n);t->busy=false;
 if(!ok||t->sink_failed){t->sink_failed=true;return fail(t);}return true;
}
static void put(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint64_t get(const uint8_t*p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(i*8);return v;}
static void hexbytes(uint8_t*b,uint64_t v){const char*d="0123456789abcdef";b[0]='0';b[1]='x';for(unsigned i=0;i<16;i++)b[2+i]=d[(v>>(60-4*i))&15];b[18]='\n';}
bool foc_init(struct foc_trace*t,void*b,size_t n,void*c,bool(*s)(void*,const char*,size_t)){
 if(!t||t->ready||t->failed||!b||!n||n>FOC_MAX_CAPACITY||!s||overlap(t,sizeof*t,b,n))return false;
 *t=(struct foc_trace){.buffer=b,.capacity=n,.context=c,.sink=s,.ready=true};return true;
}
bool foc_defer(struct foc_trace*t){if(!enter(t)||t->failed||t->deferred||t->flushed)return false;t->deferred=true;return true;}
static bool room(struct foc_trace*t,size_t n,size_t logical){
 if(t->used>t->capacity||t->capacity>FOC_MAX_CAPACITY)return fail(t);
 if(n<=t->capacity-t->used)return true;
 add(t,&t->dropped_bytes,logical);fail(t);
 if(!t->marker_attempted){t->marker_attempted=true;(void)emit(t,(const uint8_t*)FOC_OVERFLOW_MARKER,sizeof(FOC_OVERFLOW_MARKER)-1);}
 return false;
}
static bool count(struct foc_trace*t,bool hex,size_t n){
 bool ok=add(t,hex?&t->hex_calls:&t->text_calls,1);ok=add(t,&t->requested_bytes,n)&&ok;
 if(t->failed||!ok){add(t,&t->dropped_bytes,n);return false;}return true;
}
bool foc_text(struct foc_trace*t,const char*p,size_t n){
 if(!enter(t))return false;
 if(!count(t,false,n))return false;
 if((!p&&n)||overlap(p,n,t,sizeof*t)||overlap(p,n,t->buffer,t->capacity)){add(t,&t->dropped_bytes,n);return fail(t);}
 if(!t->deferred)return !n||emit(t,(const uint8_t*)p,n);
 if(t->dict_count>FOC_DICT_MAX||t->used>t->capacity)return fail(t);
 unsigned id=t->dict_count;bool found=false;
 for(unsigned i=0;i<t->dict_count;i++){
  struct foc_entry e=t->dictionary[i];
  if(e.offset<5||e.offset>t->used||e.length>t->used-e.offset||e.length>FOC_TEXT_MAX||
     t->buffer[e.offset-5]!=DEF||get(t->buffer+e.offset-4,2)!=i||
     get(t->buffer+e.offset-2,2)!=e.length)return fail(t);
  if(e.length!=n)continue;
  size_t j=0;while(j<n&&t->buffer[e.offset+j]==(uint8_t)p[j])j++;
  if(j==n){id=i;found=true;break;}
 }
 bool define=!found&&n<=FOC_TEXT_MAX&&t->dict_count<FOC_DICT_MAX;
 if(!found&&n>UINT32_MAX){add(t,&t->dropped_bytes,n);return fail(t);}
 size_t needed=found?3:5+n;
 if(!room(t,needed,n))return false;
 uint8_t*b=t->buffer+t->used;
 if(found){b[0]=REF;put(b+1,id,2);}
 else {b[0]=define?DEF:RAW;
  if(define){put(b+1,id,2);put(b+3,n,2);t->dictionary[id]=(struct foc_entry){(uint32_t)(t->used+5),(uint16_t)n};t->dict_count++;}
  else put(b+1,n,4);
  for(size_t i=0;i<n;i++)b[5+i]=(uint8_t)p[i];
 }
 t->used+=needed;return add(t,&t->encoded_bytes,needed)&&add(t,&t->accepted_events,1)&&add(t,&t->accepted_bytes,n);
}
bool foc_hex(struct foc_trace*t,uint64_t v){
 if(!enter(t)||!count(t,true,19))return false;
 if(!t->deferred){uint8_t b[19];hexbytes(b,v);return emit(t,b,19);}
 if(!room(t,9,19))return false;
 uint8_t*b=t->buffer+t->used;b[0]=HEX;put(b+1,v,8);t->used+=9;return add(t,&t->encoded_bytes,9)&&add(t,&t->accepted_events,1)&&add(t,&t->accepted_bytes,19);
}
static bool decode(struct foc_trace*t,bool output){
 struct foc_entry *dict=t->decode_dictionary;
 for(unsigned i=0;i<FOC_DICT_MAX;i++)dict[i]=(struct foc_entry){0};
 unsigned defined=0;size_t at=0;uint64_t events=0,logical=0;
 while(at<t->used){
  const uint8_t*b=t->buffer+at,*data=0;size_t left=t->used-at,n=0,step=0;uint8_t hex[19];
  switch(b[0]){
  case DEF:{if(left<5||defined==FOC_DICT_MAX)return fail(t);unsigned id=(unsigned)get(b+1,2);n=(size_t)get(b+3,2);
   if(id!=defined||n>FOC_TEXT_MAX||n>left-5)return fail(t);
   dict[defined++]=(struct foc_entry){(uint32_t)(at+5),(uint16_t)n};data=b+5;step=5+n;break;}
  case REF:{if(left<3)return fail(t);unsigned id=(unsigned)get(b+1,2);if(id>=defined)return fail(t);
   n=dict[id].length;data=t->buffer+dict[id].offset;step=3;break;}
  case RAW:if(left<5)return fail(t);n=(size_t)get(b+1,4);if(n>left-5)return fail(t);data=b+5;step=5+n;break;
  case HEX:if(left<9)return fail(t);hexbytes(hex,get(b+1,8));data=hex;n=19;step=9;break;
  default:return fail(t);
  }
  if(events==UINT64_MAX||n>UINT64_MAX-logical)return fail(t);
  events++;logical+=n;
  if(output){
   if(!emit(t,data,n))return false;
   if(!add(t,&t->decoded_events,1)||!add(t,&t->decoded_bytes,n))return false;
  }
  at+=step;
 }
 return events==t->accepted_events&&logical==t->accepted_bytes?true:fail(t);
}
bool foc_flush(struct foc_trace*t){
 if(!enter(t))return false;
 if(t->flushed)return !t->failed;
 t->flushed=true;t->deferred=false;
 if(t->sink_failed)return false;
 if(t->used>t->capacity||t->capacity>FOC_MAX_CAPACITY)return fail(t);
 /* Independent two-pass decode: no sink sees structurally malformed prefix.
  * Buffer immutable for both passes by caller contract; callbacks guarded. */
 if(!decode(t,false)||!decode(t,true))return false;
 return !t->failed;
}
