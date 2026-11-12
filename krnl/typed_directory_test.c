#include "typed_directory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct typed_directory d;
static struct td_operation op;
static uint64_t nonce;
static struct td_identity make(struct td_record *r,uint64_t owner,uint64_t life,uint64_t hpa) {
 struct td_identity id;
 assert(td_reserve(&d,&op,nonce,r,owner,life,&id));
 assert(!td_end(&d,&op,nonce));
 assert(td_attach(&d,&op,nonce,r,id,hpa|0x1e,hpa,1,0));
 assert(td_publish(&d,&op,nonce,r,id));return id;
}
static void begin(void) {assert(td_begin(&d,&op,&nonce));}
static void finish(void) {assert(td_end(&d,&op,nonce));}
static void basics(void) {
 assert(td_init(&d,1,2,3,1));begin();
 struct td_record a,b; struct td_receipt receipt;
 struct td_identity ia=make(&a,0,101,0x1000),ib=make(&b,1,102,0x2000);
 assert(ia.root_id!=ib.root_id);
 assert(td_pin(&d,&op,nonce,&a,ia));
 assert(td_refill_add(&d,&op,nonce,&a,ia));
 assert(td_resident(&d,&op,nonce,&a,ia,0));assert(td_resident(&d,&op,nonce,&a,ia,1));
 struct td_retire_ticket rt;
 assert(!td_retire_begin(&d,&op,nonce,&a,ia,&rt));
 assert(td_unpin(&d,&op,nonce,&a,ia));assert(!td_unpin(&d,&op,nonce,&a,ia));
 assert(!td_retire_begin(&d,&op,nonce,&a,ia,&rt));
 assert(td_refill_drop(&d,&op,nonce,&a,ia));
 assert(td_cache_reset(&d,&op,nonce,&a,ia,2));
 assert(!td_cache_reset(&d,&op,nonce,&a,ia,2));
 assert(td_retire_begin(&d,&op,nonce,&a,ia,&rt));
 assert(!td_pin(&d,&op,nonce,&a,ia));assert(!td_refill_add(&d,&op,nonce,&a,ia));
 assert(!td_forget(&d,&op,nonce,&a,ia));
 struct td_retire_ticket bad=rt;bad.identity.incarnation++;
 assert(!td_retire_commit(&d,&op,nonce,&a,ia,bad,3,1,&receipt));
 bad=rt;bad.cache_generation--;
 assert(!td_retire_commit(&d,&op,nonce,&a,ia,bad,3,1,&receipt));
 assert(!td_retire_commit(&d,&op,nonce,&a,ia,rt,1,1,&receipt));
 assert(!td_retire_commit(&d,&op,nonce,&a,ia,rt,3,1,NULL));
 assert(a.state==TD_RETIRING);
 assert(td_retire_commit(&d,&op,nonce,&a,ia,rt,3,1,&receipt));
 assert(!td_retire_commit(&d,&op,nonce,&a,ia,rt,3,1,&receipt));
 assert(td_forget(&d,&op,nonce,&a,ia));
 assert(!td_forget(&d,&op,nonce,&a,ia));
 struct td_identity ia2=make(&a,0,103,0x1000); /* Same storage and HPA, new allocation. */
 assert(ia2.root_incarnation!=ia.root_incarnation);
 assert(!td_pin(&d,&op,nonce,&a,ia));
 assert(!td_retire_commit(&d,&op,nonce,&a,ia2,rt,3,1,&receipt));
 finish();
 assert(td_close(&d));struct td_ticket t;
 assert(td_seal(&d,&t));struct td_snapshot s[3];size_t count=99;
 memset(s,0x5a,sizeof s);struct td_snapshot before[3];memcpy(before,s,sizeof s);
 assert(!td_snapshot_read(&d,t,s,2,&count));assert(count==99&&!memcmp(s,before,sizeof s));
 assert(td_snapshot_read(&d,t,s,3,&count)&&count==3);
 assert(s[2].receipt&&s[2].history==3&&s[2].identity.root_id==ia.root_id);
 assert(!td_begin(&d,&op,&nonce));
 struct td_ticket wrong=t;wrong.incarnation++;assert(!td_reopen(&d,wrong,2));
 assert(!td_reopen(&d,t,1));assert(td_reopen(&d,t,2));assert(!td_ticket_valid(&d,t));
 begin();assert(td_pin(&d,&op,nonce,&a,ia2));finish();
}
static void close_drain(void) {
 assert(td_init(&d,10,20,3,4));begin();
 struct td_record r;struct td_identity id;
 assert(td_reserve(&d,&op,nonce,&r,0,1,&id));
 uint64_t old=nonce;assert(td_close(&d));
 struct td_operation second;uint64_t n=42;assert(!td_begin(&d,&second,&n)&&n==42);
 struct td_ticket t;assert(!td_seal(&d,&t));assert(!td_end(&d,&op,nonce));
 /* Already admitted offline initialization is accounted until retained. */
 assert(td_attach(&d,&op,nonce,&r,id,0x301e,0x3000,0,0));
 assert(!td_publish(&d,&op,nonce,&r,id));
 assert(td_retain(&d,&op,nonce,&r,id));finish();
 assert(td_seal(&d,&t));struct td_snapshot s;size_t count;
 assert(td_snapshot_read(&d,t,&s,1,&count)&&count==1&&s.state==TD_RETAINED&&s.cache_generation==0&&s.history==0);
 assert(td_reopen(&d,t,5));begin();assert(nonce!=old);assert(!td_end(&d,&op,old));
 struct td_retire_ticket rt;struct td_receipt receipt;
 assert(td_retire_begin(&d,&op,nonce,&r,id,&rt));
 assert(td_retire_commit(&d,&op,nonce,&r,id,rt,0,99,&receipt));assert(td_forget(&d,&op,nonce,&r,id));finish();
}
static void retained_and_identity(void) {
 assert(td_init(&d,1,1,3,1));begin();struct td_record r;
 struct td_identity id;assert(td_reserve(&d,&op,nonce,&r,0,100,&id));
 struct td_identity wrong=id;wrong.allocator_lifetime++;
 assert(!td_attach(&d,&op,nonce,&r,wrong,0x401e,0x4000,1,0));
 assert(!td_attach(&d,&op,nonce,&r,id,0x401e,0x4000,1,1));
 assert(td_cancel(&d,&op,nonce,&r,id));assert(!td_cancel(&d,&op,nonce,&r,id));
 id=make(&r,0,101,0x4000);
 assert(!td_cancel(&d,&op,nonce,&r,id));assert(!td_resident(&d,&op,nonce,&r,id,2));
 struct td_retire_ticket old,now;struct td_receipt receipt;
 assert(td_retire_begin(&d,&op,nonce,&r,id,&old));
 assert(td_retain(&d,&op,nonce,&r,id));assert(td_retire_begin(&d,&op,nonce,&r,id,&now));
 assert(now.nonce!=old.nonce);
 assert(!td_retire_commit(&d,&op,nonce,&r,id,old,0,8,&receipt));
 assert(td_retire_commit(&d,&op,nonce,&r,id,now,0,9,&receipt));finish();
}
static void unresolved_binding(void) {
 assert(td_init(&d,1,2,3,1));begin();struct td_record r;struct td_identity id;
 assert(td_reserve(&d,&op,nonce,&r,0,1,&id));
 assert(td_retain(&d,&op,nonce,&r,id));finish();
 begin();struct td_retire_ticket rt;assert(!td_retire_begin(&d,&op,nonce,&r,id,&rt));finish();
 assert(td_close(&d));struct td_ticket t;assert(!td_seal(&d,&t));
}
static void retirement_freeze_order(void) {
 assert(td_init(&d,1,2,3,1));begin();struct td_record r;
 struct td_identity id=make(&r,0,1,0x1000);struct td_retire_ticket rt;
 /* close wins: an admitted operation cannot start a retirement afterward. */
 assert(td_close(&d));assert(!td_retire_begin(&d,&op,nonce,&r,id,&rt));finish();
 struct td_ticket t;assert(td_seal(&d,&t));assert(td_reopen(&d,t,2));
 /* retirement wins: close must leave the gate open for evidence retry. */
 begin();assert(td_retire_begin(&d,&op,nonce,&r,id,&rt));finish();
 uint64_t n=d.next_nonce;assert(!td_close(&d)&&!d.closed&&d.next_nonce==n&&d.type_epoch==2);
 begin();struct td_receipt receipt;
 assert(td_retire_commit(&d,&op,nonce,&r,id,rt,0,7,&receipt));
 assert(td_forget(&d,&op,nonce,&r,id));finish();
 assert(td_close(&d)&&td_seal(&d,&t)&&td_reopen(&d,t,3));
}
static void limits_and_poison(void) {
 assert(td_init(&d,1,1,3,1));d.next_nonce=UINT64_MAX;
 uint64_t n=7;assert(!td_begin(&d,&op,&n)&&n==7);assert(!td_close(&d));
 assert(td_init(&d,1,1,3,1));begin();struct td_record r;struct td_identity id;
 d.next_id=UINT64_MAX;assert(!td_reserve(&d,&op,nonce,&r,0,1,&id));d.next_id=0;
 id=make(&r,0,1,0x1000);uint64_t v=d.version;d.version=UINT64_MAX;
 assert(!td_pin(&d,&op,nonce,&r,id)&&r.pins==0);d.version=v;
 td_poison(&d);assert(!td_pin(&d,&op,nonce,&r,id));assert(td_end(&d,&op,nonce));
 struct td_ticket t;assert(!td_seal(&d,&t));assert(!td_begin(&d,&op,&n));
 assert(td_init(&d,1,1,3,UINT64_MAX));assert(td_close(&d));assert(td_seal(&d,&t));assert(!td_reopen(&d,t,1));
}
int main(void) {basics();close_drain();retained_and_identity();unresolved_binding();retirement_freeze_order();limits_and_poison();puts("typed_directory PASS");return 0;}
