#include "bounded_span.h"
bool vbs_init(struct vbs *s,uint64_t ram,uint64_t gpa,uint64_t n){
 if(!s||gpa>ram||n>ram-gpa)return false;
 s->cursor=gpa;s->remaining=n;return true;
}
bool vbs_next(struct vbs *s,uint64_t *page,size_t *off,size_t *len){
 if(!s||!page||!off||!len||!s->remaining)return false;
 size_t o=(size_t)(s->cursor&4095),n=4096-o;
 if(s->remaining<n)n=(size_t)s->remaining;
 /* The admitted half-open end cannot exceed UINT64_MAX. */
 if(s->cursor>UINT64_MAX-n)return false;
 *page=s->cursor/4096;*off=o;*len=n;
 s->cursor+=n;s->remaining-=n;return true;
}
