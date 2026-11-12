#include "../domain_typed_lease.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct dtl_inventory inventory={.domain=1,.incarnation=2,.epoch=1,.participants=3,
 .host={0x8000,0x9000,0x8000000000008003,8},.roots=2,.aliases=1,.refills=2,
 .root={{1,1,0x10000,0,3,99,0,true,1},{2,1,0x11000,1,2,98,3,true,0}},
 .alias={{1,1,0x2000,0x8000,0x8073,6,3,true,3}},.refill={{1,1,1,0,1},{2,1,2,1,1}},
 .complete=true,.dedicated_data_wb=true};
static void phase_failures(struct domain_typed_lease*l,struct dtl_ticket k){
 struct domain_typed_lease poison=*l,before=*l;
 dtl_fail(&poison);
 assert(poison.phase==DTL_POISON&&poison.pins==l->pins&&poison.tokens==l->tokens);
 assert(!dtl_advance(&poison,k,l->phase)&&!dtl_release_ready(&poison,3,3,3));
 struct dtl_ticket bad=k;
 for(unsigned f=0;f<5;f++){
  bad=k;
  switch(f){case 0:bad.domain++;break;case 1:bad.incarnation++;break;case 2:bad.epoch++;break;
   case 3:bad.source^=6;break;case 4:bad.destination^=6;break;}
  assert(!dtl_advance(l,bad,l->phase));assert(!memcmp(l,&before,sizeof before));
 }
 assert(!dtl_cpu_ack(l,k,l->phase,63));assert(!memcmp(l,&before,sizeof before));
}
static void cycle(struct domain_typed_lease*l,struct dtl_token*t,uint64_t nonce,unsigned dest){
 struct dtl_ticket k;assert(dtl_begin(l,dest,3,3,3,&k));
 assert(!dtl_admit(l,t,0,0,nonce,t->epoch,t->type,true,false));
 phase_failures(l,k);
 struct dtl_ticket bad=k;bad.incarnation++;
 assert(!dtl_detach(l,bad,0));assert(!dtl_advance(l,k,DTL_DETACH));
 for(unsigned i=0;i<2;i++){assert(dtl_detach(l,k,i));assert(dtl_revoke(l,k,i));}
 assert(dtl_root_ack(l,k,0,0));assert(!dtl_root_ack(l,k,0,0));assert(dtl_root_ack(l,k,0,1));assert(dtl_root_ack(l,k,1,1));
 assert(dtl_advance(l,k,DTL_DETACH));phase_failures(l,k);assert(dtl_advance(l,k,DTL_BREAK));
 for(unsigned p=DTL_TLB1;p<=DTL_CACHE2;p++){
  phase_failures(l,k);
  if(p==DTL_MAKE){assert(dtl_advance(l,k,p));continue;}
  assert(!dtl_advance(l,k,p));assert(dtl_cpu_ack(l,k,p,0));assert(dtl_cpu_ack(l,k,p,1));assert(dtl_advance(l,k,p));
 }
 phase_failures(l,k);
 for(unsigned r=0;r<2;r++)assert(dtl_publish_root(l,k,r));
 assert(dtl_root_ack(l,k,0,0));assert(dtl_root_ack(l,k,0,1));assert(dtl_root_ack(l,k,1,1));
 assert(dtl_advance(l,k,DTL_PUBLISH));phase_failures(l,k);assert(!dtl_advance(l,k,DTL_STAMP));
 assert(dtl_prepare_token(l,k,t,nonce));assert(dtl_advance(l,k,DTL_STAMP));
 assert(dtl_admit(l,t,0,0,nonce,l->epoch,dest,true,false));
 assert(!dtl_admit(l,t,0,0,nonce,l->epoch-1,dest,true,false));
 assert(!dtl_advance(l,k,DTL_STAMP));
}
int main(void){
 struct domain_typed_lease l;struct dtl_token t={0};uint64_t nonce;
 assert(dtl_init(&l,&inventory));assert(dtl_acquire(&l,0,0,1,6,true,false,&t,&nonce));
 assert(!dtl_admit(&l,&t,0,1,nonce,1,6,true,false));
 assert(!dtl_release(&l,&t,0,nonce,3,3));
 cycle(&l,&t,nonce,0);assert(!dtl_release_ready(&l,3,3,3));cycle(&l,&t,nonce,6);
 assert(!dtl_retire_root(&l,0,false,true,true));assert(dtl_retire_root(&l,0,true,true,true));
 assert(dtl_retire_root(&l,1,true,true,true));assert(dtl_release(&l,&t,0,nonce,3,3));assert(!dtl_release(&l,&t,0,nonce,3,3));
 assert(dtl_release_ready(&l,3,3,3));dtl_fail(&l);assert(!dtl_release_ready(&l,3,3,3));
 struct dtl_inventory bad=inventory;bad.roots=DTL_ROOTS+1;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.alias[0].perms=7;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.root[1].scan_digest=0;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.aliases=0;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.root[1].scanned_aliases=1;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.alias[0].effective_perms=0;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.alias[0].effective_perms=7;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.alias[0].perms=1;bad.alias[0].raw=0x8071;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.alias[0].effective_perms=1;assert(dtl_init(&l,&bad));
 assert(dtl_init(&l,&inventory));struct domain_typed_lease before=l;
 struct dtl_ticket unchanged={99,99,99,99,99},out=unchanged;
 assert(!dtl_begin(&l,0,1,3,3,&out)&&!memcmp(&before,&l,sizeof l)&&!memcmp(&out,&unchanged,sizeof out));
 assert(!dtl_begin(&l,0,3,1,3,&out));assert(!dtl_begin(&l,0,3,3,1,&out));
 l.epoch=UINT64_MAX;assert(!dtl_begin(&l,0,3,3,3,&out));
 assert(dtl_init(&l,&inventory));l.nonce=UINT64_MAX;struct dtl_token fresh={0};
 assert(!dtl_acquire(&l,0,0,1,6,true,false,&fresh,&nonce)&&!fresh.live&&!l.pins);
 bad=inventory;bad.aliases=DTL_ALIASES+1;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.refills=DTL_REFILLS+1;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.refill[0].root_generation++;assert(!dtl_init(&l,&bad));
 bad=inventory;bad.root[0].owner=63;assert(!dtl_init(&l,&bad));
 assert(!dtl_init_type(&l,&inventory,1));
 assert(dtl_init(&l,&inventory));assert(!dtl_begin(&l,4,3,3,3,&out));
 assert(dtl_init_type(&l,&inventory,4));
 assert(!dtl_begin(&l,0,3,3,3,&out));
 struct dtl_token wt={0};uint64_t wt_nonce;
 assert(dtl_acquire(&l,0,0,1,6,true,false,&wt,&wt_nonce));
 cycle(&l,&wt,wt_nonce,4);
 assert(!dtl_begin(&l,0,3,3,3,&out)); /* no WT->UC */
 assert(!dtl_retire_root(&l,0,true,true,true));
 assert(!dtl_release_ready(&l,3,3,3));
 cycle(&l,&wt,wt_nonce,6);
 assert(dtl_admit(&l,&wt,0,0,wt_nonce,l.epoch,6,true,false));
 assert(dtl_init(&l,&inventory));
 uint64_t rid=l.inventory.root[0].id,rgen=l.inventory.root[0].generation;
 assert(dtl_root_residency_extend(&l,0,rid,rgen,3));
 struct domain_typed_lease preserved=l;
 assert(!dtl_root_residency_extend(&l,0,rid,rgen,1));
 assert(!dtl_root_residency_extend(&l,0,rid+1,rgen,3));
 assert(!dtl_root_residency_extend(&l,0,rid,rgen+1,3));
 assert(!dtl_root_residency_extend(&l,0,rid,rgen,7));
 assert(!memcmp(&l,&preserved,sizeof l));
 assert(dtl_begin(&l,0,3,3,3,&out));
 assert(!dtl_root_residency_extend(&l,0,rid,rgen,3));
 puts("domain_typed_lease: PASS");
}
