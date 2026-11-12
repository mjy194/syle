#include "typed_root_lease.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct fixture {
 struct typed_directory d;
 struct typed_root_lease l;
 struct td_operation op;
 uint64_t n;
 struct td_record r[8];
 struct trl_binding b[8];
 struct td_receipt receipt[8];
};
static void init(struct fixture *f,size_t budget) {
 memset(f,0,sizeof *f);
 assert(td_init(&f->d,10,20,3,1));
 assert(trl_init(&f->l,&f->d,(struct trl_backing){0x800000,900,true},budget));
}
static void begin(struct fixture *f) {assert(td_begin(&f->d,&f->op,&f->n));}
static void end(struct fixture *f) {assert(td_end(&f->d,&f->op,f->n));}
static struct trl_token tok(struct fixture *f,unsigned i) {
 struct trl_token t;assert(trl_token_read(&f->l,&f->b[i],&t));return t;
}
static void reserve(struct fixture *f,unsigned i,uint64_t owner,uint64_t life) {
 assert(trl_reserve(&f->l,&f->op,f->n,&f->b[i],owner,life));
 struct td_identity id;
 assert(td_reserve(&f->d,&f->op,f->n,&f->r[i],owner,life,&id));
 assert(trl_bind_candidate(&f->l,&f->op,f->n,&f->b[i],&f->r[i],id));
}
static void attach(struct fixture *f,unsigned i,uint64_t hpa,uint64_t generation) {
 assert(td_attach(&f->d,&f->op,f->n,&f->r[i],f->b[i].token.identity,hpa|0x1e,hpa,generation,0));
 assert(trl_attach(&f->l,&f->op,f->n,&f->b[i]));
}
static void publish(struct fixture *f,unsigned i) {
 assert(trl_publish_ready(&f->l,&f->op,f->n,&f->b[i]));
 assert(td_publish(&f->d,&f->op,f->n,&f->r[i],f->b[i].token.identity));
 assert(trl_publish(&f->l,&f->op,f->n,&f->b[i]));
}
static void make(struct fixture *f,unsigned i,uint64_t owner,uint64_t life,uint64_t hpa) {
 reserve(f,i,owner,life);attach(f,i,hpa,1);publish(f,i);
}
static struct trl_freeze freeze(struct fixture *f) {
 struct trl_freeze t;assert(trl_close(&f->l));assert(trl_seal(&f->l,&t));return t;
}
static struct trl_transition_evidence evidence(struct fixture *f,struct trl_freeze t,uint64_t epoch,unsigned destination) {
 uint64_t all=f->d.participants;
 return (struct trl_transition_evidence){.freeze=t,.new_epoch=epoch,.serial=55,
  .closed=all,.drained=all,.tlb_before=all,.cache_before=all,.tlb_after=all,.cache_after=all,
  .destination=destination,.complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,
  .host_break_before_make=true,.all_aliases_retyped=true,.owner_stamps_ready=true};
}
static void transition(struct fixture *f,unsigned type) {
 struct trl_freeze t=freeze(f);uint64_t epoch=f->l.type_epoch+1;
 struct trl_token prepared;
 for(unsigned i=0;i<8;i++)if(f->b[i].token_live)
  assert(trl_prepare_token(&f->l,t,&f->b[i],tok(f,i),epoch,type,&prepared));
 struct trl_transition_evidence e=evidence(f,t,epoch,type);
 assert(trl_reopen(&f->l,t,&e));assert(!trl_freeze_valid(&f->l,t));
}
static void retire(struct fixture *f,unsigned i) {
 struct trl_token t=tok(f,i);struct td_retire_ticket rt;
 assert(td_retire_begin(&f->d,&f->op,f->n,&f->r[i],t.identity,&rt));
 assert(td_retire_commit(&f->d,&f->op,f->n,&f->r[i],t.identity,rt,rt.history,77,&f->receipt[i]));
 assert(trl_retire(&f->l,&f->op,f->n,&f->b[i],t,rt,&f->receipt[i],f->d.version));
 assert(!trl_retire(&f->l,&f->op,f->n,&f->b[i],t,rt,&f->receipt[i],f->d.version));
 assert(td_forget(&f->d,&f->op,f->n,&f->r[i],t.identity));
}
static void reservation_and_tokens(void) {
 struct fixture f;init(&f,2);begin(&f);
 assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[0],2,1));
 assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[0],0,0));
 assert(trl_reserve(&f.l,&f.op,f.n,&f.b[0],0,1));
 assert(!f.d.records&&f.d.next_id==0&&f.l.reserved==1&&!f.l.live_tokens);
 assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[0],0,1));
 assert(trl_cancel(&f.l,&f.op,f.n,&f.b[0]));
 reserve(&f,0,0,10);struct trl_token old=tok(&f,0);
 assert(!trl_cancel(&f.l,&f.op,f.n,&f.b[0])); /* directory cancellation first */
 assert(td_cancel(&f.d,&f.op,f.n,&f.r[0],old.identity));
 assert(trl_cancel(&f.l,&f.op,f.n,&f.b[0]));
 assert(!trl_cancel(&f.l,&f.op,f.n,&f.b[0]));
 make(&f,0,0,11,0x1000);struct trl_token a=tok(&f,0);
 assert(a.nonce!=old.nonce&&a.identity.root_id!=old.identity.root_id);
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],old,TRL_WB,true,false));
 assert(trl_admit(&f.l,&f.op,f.n,&f.b[0],a,TRL_WB,true,false));
 struct trl_binding copy=f.b[0];
 assert(!trl_admit(&f.l,&f.op,f.n,&copy,a,TRL_WB,true,false));
 assert(!trl_admit(&f.l,&f.op,f.n+1,&f.b[0],a,TRL_WB,true,false));
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],a,TRL_UC,true,false));
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],a,4,true,false));
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],a,TRL_WB,false,false));
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],a,TRL_WB,true,true));
 struct trl_token bad=a;
#define BAD_TOKEN(field) do {bad=a;bad.field++;assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],bad,TRL_WB,true,false));} while(0)
 BAD_TOKEN(identity.domain);BAD_TOKEN(identity.incarnation);BAD_TOKEN(identity.root_id);
 BAD_TOKEN(identity.root_incarnation);BAD_TOKEN(identity.allocator_lifetime);BAD_TOKEN(identity.owner);
 BAD_TOKEN(nonce);BAD_TOKEN(type_epoch);BAD_TOKEN(type);
#undef BAD_TOKEN
 make(&f,1,1,12,0x2000);
 uint64_t id=f.d.next_id,version=f.l.version;
 assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[2],0,13));
 assert(f.d.next_id==id&&f.l.version==version);
 assert(td_cache_reset(&f.d,&f.op,f.n,&f.r[0],a.identity,2));
 assert(trl_admit(&f.l,&f.op,f.n,&f.b[0],a,TRL_WB,true,false));
 assert(tok(&f,0).nonce==a.nonce);
 retire(&f,0);
 assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[2],0,13)); /* receipt consumes budget */
 assert(!trl_token_read(&f.l,&f.b[0],&bad));end(&f);
}
static void receipt_integrity(void) {
 struct fixture f;init(&f,3);begin(&f);make(&f,0,0,10,0x1000);
 struct trl_token t=tok(&f,0);struct td_retire_ticket rt;
 assert(td_pin(&f.d,&f.op,f.n,&f.r[0],t.identity));
 assert(!td_retire_begin(&f.d,&f.op,f.n,&f.r[0],t.identity,&rt));
 assert(td_unpin(&f.d,&f.op,f.n,&f.r[0],t.identity));
 assert(td_refill_add(&f.d,&f.op,f.n,&f.r[0],t.identity));
 assert(!td_retire_begin(&f.d,&f.op,f.n,&f.r[0],t.identity,&rt));
 assert(td_refill_drop(&f.d,&f.op,f.n,&f.r[0],t.identity));
 assert(td_resident(&f.d,&f.op,f.n,&f.r[0],t.identity,0));
 assert(td_resident(&f.d,&f.op,f.n,&f.r[0],t.identity,1));
 assert(td_cache_reset(&f.d,&f.op,f.n,&f.r[0],t.identity,2));
 assert(td_retire_begin(&f.d,&f.op,f.n,&f.r[0],t.identity,&rt));
 assert(!td_retire_commit(&f.d,&f.op,f.n,&f.r[0],t.identity,rt,1,11,&f.receipt[0]));
 assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],f.d.version));
 assert(td_retire_commit(&f.d,&f.op,f.n,&f.r[0],t.identity,rt,3,11,&f.receipt[0]));
 uint64_t commit_version=f.d.version;
 struct td_receipt copied=f.receipt[0];
 assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&copied,commit_version));
 assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],0));
 assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],f.d.version+1));
 struct td_retire_ticket bad=rt;
#define BAD_TICKET(field) do {bad=rt;bad.field++;assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,bad,&f.receipt[0],commit_version));} while(0)
 BAD_TICKET(identity.domain);BAD_TICKET(identity.incarnation);BAD_TICKET(identity.root_id);
 BAD_TICKET(identity.root_incarnation);BAD_TICKET(identity.allocator_lifetime);BAD_TICKET(identity.owner);
 BAD_TICKET(full_eptp);BAD_TICKET(hpa);BAD_TICKET(cache_generation);BAD_TICKET(history);BAD_TICKET(type_epoch);BAD_TICKET(nonce);
#undef BAD_TICKET
 f.r[0].pins=1;assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],commit_version));f.r[0].pins=0;
 f.r[0].refills=1;assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],commit_version));f.r[0].refills=0;
 f.receipt[0].acked=1;assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],commit_version));f.receipt[0].acked=3;
 f.receipt[0].cache_generation++;assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],commit_version));f.receipt[0].cache_generation--;
 /* A postcommit binding failure retains the token; retry uses the original
  * commit version, not the later directory version. */
 uint64_t lv=f.l.version;f.l.version=UINT64_MAX;
 assert(!trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],commit_version));
 assert(f.l.live_tokens==1&&f.b[0].record==&f.r[0]);f.l.version=lv;
 make(&f,1,1,11,0x2000);assert(f.d.version>commit_version);
 assert(trl_retire(&f.l,&f.op,f.n,&f.b[0],t,rt,&f.receipt[0],commit_version));
 assert(f.b[0].receipt_version==commit_version&&!f.b[0].record);
 end(&f);
 struct trl_freeze frozen=freeze(&f);struct trl_snapshot s[3];size_t count=0;
 assert(trl_snapshot_read(&f.l,frozen,s,3,&count)&&count==3);
 unsigned old_entries=0;
 for(size_t i=0;i<count;i++)if(s[i].root.identity.root_id==t.identity.root_id){assert(!s[i].scan_allowed);old_entries++;}
 assert(old_entries==2); /* retired-but-not-forgotten plus durable receipt */
}
static void aba_and_epoch(void) {
 struct fixture f;init(&f,4);begin(&f);make(&f,0,0,1,0x1000);
 struct trl_token old=tok(&f,0);retire(&f,0);
 /* Destroy old record bytes to prove receipt-only snapshots never read it. */
 memset(&f.r[0],0xa5,sizeof f.r[0]);
 make(&f,1,0,2,0x1000);struct trl_token now=tok(&f,1);
 assert(now.identity.root_id!=old.identity.root_id&&now.nonce!=old.nonce);
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[1],old,TRL_WB,true,false));end(&f);
 struct trl_freeze t=freeze(&f);struct trl_snapshot s[2],before[2];
 memset(s,0x5a,sizeof s);memcpy(before,s,sizeof s);size_t count=44,index=99;
 assert(!trl_snapshot_read(&f.l,t,s,1,&count));assert(count==44&&!memcmp(s,before,sizeof s));
 assert(trl_snapshot_read(&f.l,t,s,2,&count)&&count==2);
 assert(trl_snapshot_index(&f.l,t,now.identity,false,&index)&&index==0);
 assert(s[index].scan_allowed&&s[index].root.hpa==0x1000);
 assert(trl_snapshot_index(&f.l,t,old.identity,true,&index)&&index==1);
 assert(!s[index].scan_allowed&&s[index].root.hpa==0x1000&&s[index].receipt_type_epoch==1);
 assert(!trl_snapshot_index(&f.l,t,old.identity,false,&index));
 struct trl_freeze wrong=t;wrong.directory.version++;assert(!trl_freeze_valid(&f.l,wrong));
 wrong=t;wrong.lease_version++;assert(!trl_freeze_valid(&f.l,wrong));
 wrong=t;wrong.backing.allocator_lifetime++;assert(!trl_freeze_valid(&f.l,wrong));
 wrong=t;wrong.backing.hpa+=4096;assert(!trl_freeze_valid(&f.l,wrong));
 struct trl_token next;
 struct trl_transition_evidence e=evidence(&f,t,2,TRL_UC);
 assert(!trl_reopen(&f.l,t,&e)); /* all outstanding tokens must be prepared */
 assert(!trl_prepare_token(&f.l,t,&f.b[1],old,2,TRL_UC,&next));
 assert(!trl_prepare_token(&f.l,t,&f.b[1],now,1,TRL_UC,&next));
 assert(!trl_prepare_token(&f.l,t,&f.b[1],now,2,4,&next));
 assert(trl_prepare_token(&f.l,t,&f.b[1],now,2,TRL_UC,&next));
 assert(next.nonce==now.nonce&&next.identity.root_id==now.identity.root_id);
 assert(!trl_prepare_token(&f.l,t,&f.b[1],now,2,TRL_UC,&next));
 struct trl_transition_evidence bad=e;bad.cache_after=1;assert(!trl_reopen(&f.l,t,&bad));
 bad=e;bad.freeze.backing.allocator_lifetime++;assert(!trl_reopen(&f.l,t,&bad));
 bad=e;bad.complete_alias_audit=false;assert(!trl_reopen(&f.l,t,&bad));
 assert(trl_reopen(&f.l,t,&e));assert(!trl_freeze_valid(&f.l,t));
 begin(&f);assert(!trl_admit(&f.l,&f.op,f.n,&f.b[1],now,TRL_UC,true,false));
 assert(trl_admit(&f.l,&f.op,f.n,&f.b[1],next,TRL_UC,true,false));
 make(&f,2,1,3,0x3000);assert(tok(&f,2).type==TRL_UC&&tok(&f,2).type_epoch==2);end(&f);
 t=freeze(&f);struct trl_snapshot all[3];
 assert(trl_snapshot_read(&f.l,t,all,3,&count)&&count==3);
 assert(trl_snapshot_index(&f.l,t,now.identity,false,&index)&&index==1); /* order changed */
 assert(all[index].token.nonce==now.nonce);
 assert(trl_snapshot_index(&f.l,t,old.identity,true,&index)&&index==2);
 assert(all[index].receipt_type_epoch==1&&all[index].receipt_version==f.b[0].receipt_version&&!all[index].scan_allowed);
 uint64_t saved=f.b[0].receipt_version;f.b[0].receipt_version=f.d.version+1;
 assert(!trl_snapshot_read(&f.l,t,all,3,&count));f.b[0].receipt_version=saved;
 uint64_t saved_epoch=f.b[0].retirement.type_epoch;f.b[0].retirement.type_epoch=3;
 assert(!trl_snapshot_read(&f.l,t,all,3,&count));f.b[0].retirement.type_epoch=saved_epoch;
 f.receipt[0].flush_serial++;
 assert(!trl_snapshot_read(&f.l,t,all,3,&count));f.receipt[0].flush_serial--;
}
static void failed_candidate_and_drain(void) {
 struct fixture f;init(&f,4);begin(&f);reserve(&f,0,0,1);
 uint64_t version=f.d.version,epoch=f.d.type_epoch;
 assert(!trl_close(&f.l)&&!f.d.closed&&f.d.version==version&&f.d.type_epoch==epoch);
 attach(&f,0,0x1000,0);assert(!trl_publish_ready(&f.l,&f.op,f.n,&f.b[0]));
 assert(!td_publish(&f.d,&f.op,f.n,&f.r[0],f.b[0].token.identity));
 assert(td_retain(&f.d,&f.op,f.n,&f.r[0],f.b[0].token.identity));
 assert(trl_retain(&f.l,&f.op,f.n,&f.b[0]));end(&f);
 assert(!trl_close(&f.l)&&!f.d.closed); /* cleanup still admitted */
 begin(&f);retire(&f,0);make(&f,1,1,2,0x2000);
 assert(td_refill_add(&f.d,&f.op,f.n,&f.r[1],f.b[1].token.identity));end(&f);
 assert(!trl_close(&f.l)&&!f.d.closed);
 begin(&f);assert(td_refill_drop(&f.d,&f.op,f.n,&f.r[1],f.b[1].token.identity));end(&f);
 begin(&f);struct td_retire_ticket rt;struct trl_token token=tok(&f,1);
 assert(td_retire_begin(&f.d,&f.op,f.n,&f.r[1],token.identity,&rt));end(&f);
 version=f.d.version;epoch=f.d.type_epoch;
 assert(!trl_close(&f.l)&&!f.d.closed&&f.d.version==version&&f.d.type_epoch==epoch);
 begin(&f);assert(td_retire_commit(&f.d,&f.op,f.n,&f.r[1],token.identity,rt,0,22,&f.receipt[1]));
 uint64_t cv=f.d.version;
 /* Legacy delegate may already forget the stable record, but guarded free
  * must still wait for the model token to accept the durable receipt. */
 assert(td_forget(&f.d,&f.op,f.n,&f.r[1],token.identity));
 assert(trl_retire(&f.l,&f.op,f.n,&f.b[1],token,rt,&f.receipt[1],cv));end(&f);
 begin(&f);assert(!trl_close(&f.l)&&!f.d.closed);end(&f);
 struct trl_freeze t=freeze(&f);struct td_operation another;uint64_t n=0;
 assert(!td_begin(&f.d,&another,&n));assert(!trl_close(&f.l));
 struct trl_snapshot snapshots[2];size_t count;
 assert(trl_snapshot_read(&f.l,t,snapshots,2,&count)&&count==2);
 assert(!snapshots[0].scan_allowed&&!snapshots[1].scan_allowed);
}
static void uc_retirement_not_backing_free(void) {
 struct fixture f;init(&f,4);begin(&f);make(&f,0,0,1,0x1000);make(&f,1,1,2,0x2000);end(&f);
 transition(&f,TRL_UC);begin(&f);retire(&f,0);
 assert(f.l.type==TRL_UC&&f.l.live_tokens==1);retire(&f,1);end(&f);
 assert(f.l.type==TRL_UC&&!f.l.live_tokens);
 struct trl_freeze t=freeze(&f);
 struct trl_final_evidence e={.freeze=t,.serial=77,.parked=3,.vmcs_gone=3,.refills_revoked=3,
  .roots_flushed=3,.caches_flushed=3,.restored_wb=true,.host_wb_verified=true,.complete_alias_audit=true};
 assert(!trl_backing_release_ready(&f.l,t,&e)); /* no tokens is insufficient in UC */
 struct trl_transition_evidence restore=evidence(&f,t,3,TRL_WB);
 assert(trl_reopen(&f.l,t,&restore)); /* no live tokens to prepare, receipts stay epoch 2 */
 t=freeze(&f);e.freeze=t;
 assert(trl_backing_release_ready(&f.l,t,&e));
 e.host_wb_verified=false;assert(!trl_backing_release_ready(&f.l,t,&e));e.host_wb_verified=true;
 e.restored_wb=false;assert(!trl_backing_release_ready(&f.l,t,&e));e.restored_wb=true;
 e.parked=1;assert(!trl_backing_release_ready(&f.l,t,&e));e.parked=3;
 e.vmcs_gone=1;assert(!trl_backing_release_ready(&f.l,t,&e));e.vmcs_gone=3;
 e.refills_revoked=1;assert(!trl_backing_release_ready(&f.l,t,&e));e.refills_revoked=3;
 e.roots_flushed=1;assert(!trl_backing_release_ready(&f.l,t,&e));e.roots_flushed=3;
 e.caches_flushed=1;assert(!trl_backing_release_ready(&f.l,t,&e));e.caches_flushed=3;
 e.freeze.backing.allocator_lifetime++;assert(!trl_backing_release_ready(&f.l,t,&e));
 struct trl_snapshot s[2];size_t count;
 assert(trl_snapshot_read(&f.l,t,s,2,&count)&&count==2);
 assert(s[0].receipt_type_epoch==2&&s[1].receipt_type_epoch==2);
 assert(!s[0].scan_allowed&&!s[1].scan_allowed);
}
static void limits_and_poison(void) {
 struct fixture f;init(&f,2);begin(&f);
 uint64_t version=f.l.version;
 f.l.version=UINT64_MAX;assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[0],0,1));f.l.version=version;
 f.l.next_nonce=UINT64_MAX;assert(!trl_reserve(&f.l,&f.op,f.n,&f.b[0],0,1));f.l.next_nonce=0;
 make(&f,0,0,1,0x1000);struct trl_token t=tok(&f,0);
 trl_poison(&f.l);assert(f.d.poisoned&&f.d.closed);
 assert(!trl_admit(&f.l,&f.op,f.n,&f.b[0],t,TRL_WB,true,false));
 assert(!trl_close(&f.l));assert(f.l.live_tokens==1&&f.b[0].record==&f.r[0]);end(&f);
 struct typed_directory d;struct typed_root_lease l;
 assert(td_init(&d,1,1,3,1));
 assert(!trl_init(&l,&d,(struct trl_backing){0x800000,1,false},1));
 assert(!trl_init(&l,&d,(struct trl_backing){0x800001,1,true},1));
 assert(!trl_init(&l,&d,(struct trl_backing){0x800000,0,true},1));
 assert(!trl_init(&l,&d,(struct trl_backing){0x800000,1,true},0));
}
int main(void) {
 reservation_and_tokens();receipt_integrity();aba_and_epoch();failed_candidate_and_drain();
 uc_retirement_not_backing_free();limits_and_poison();
 puts("typed root lease tests passed");return 0;
}
