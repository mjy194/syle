#include "source_session.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 static unsigned char bytes[3][16];
 struct src_span spans[3]={{bytes[0],16},{bytes[1],16},{bytes[2],16}};
 struct src_owner s={0};struct src_ref r={0,99};
 assert(!src_init(&s,0)&&!s.initialized&&src_init(&s,spans));
 assert(src_acquire(&s,&r));struct src_ref first=r,sentinel={0,123};
 assert(!src_acquire(&s,&sentinel)&&!sentinel.issuer&&sentinel.generation==123);
 assert(src_contains(r,0,bytes[0]+15,1)&&!src_contains(r,0,bytes[0]+15,2));
 assert(!src_contains(r,3,bytes[0],1)&&!src_contains(r,0,bytes[1],1));
 assert(!src_replace(&s,spans)&&!src_retire(&s));
 assert(src_release(r)&&!src_release(r)&&src_acquire(&s,&r));
 assert(!src_contains(first,0,bytes[0],1)&&!src_release(first)&&s.held);
 assert(src_release(r)&&src_replace(&s,spans)&&src_acquire(&s,&r));
 assert(src_release(r));s.generation=UINT64_MAX; /* isolated private corruption/exhaustion fixture */
 sentinel=(struct src_ref){0,123};assert(!src_acquire(&s,&sentinel)&&!s.held&&!sentinel.issuer&&sentinel.generation==123);
 assert(!src_replace(&s,spans)&&s.generation==UINT64_MAX);
 assert(src_retire(&s)&&!src_init(&s,spans)&&!src_acquire(&s,&r));
 puts("source owner same-address ref ABA, exhaustion and failure outputs PASS");return 0;
}
