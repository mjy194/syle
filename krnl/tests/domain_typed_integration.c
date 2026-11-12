#include "../domain_typed_inventory.h"
#include "../domain_typed_lease.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ROOTS 6u
static _Alignas(4096) uint64_t tables[ROOTS][4][512];
static struct domain_typed_workspace workspace;
static struct domain_typed_inventory audit;
static uint64_t root_hpa(unsigned r){return 0x100000+r*0x4000;}
static bool resolve(void*c,uint64_t h,const uint64_t **v){
 (void)c;if(h<0x100000||h>=root_hpa(ROOTS)||(h&4095))return false;
 unsigned n=(unsigned)((h-0x100000)/4096);*v=tables[n/4][n%4];return true;
}
static void transition(struct domain_typed_lease*l,struct dtl_token*t,uint64_t*nonce,unsigned dest){
 struct dtl_ticket k;assert(dtl_begin(l,dest,3,3,3,&k));
 for(unsigned r=0;r<ROOTS;r++){assert(dtl_detach(l,k,r));assert(dtl_revoke(l,k,r));}
 /* Historical root5 has no target leaf today but BOTH CPUs must invalidate. */
 for(unsigned r=0;r<ROOTS;r++)for(unsigned cpu=0;cpu<2;cpu++)
  if(l->inventory.root[r].residency&(1u<<cpu)){
   if(r==5&&cpu==1)continue;
   assert(dtl_root_ack(l,k,r,cpu));
  }
 assert(!dtl_advance(l,k,DTL_DETACH));
 assert(dtl_root_ack(l,k,5,1));assert(dtl_advance(l,k,DTL_DETACH));
 assert(dtl_advance(l,k,DTL_BREAK));
 for(unsigned phase=DTL_TLB1;phase<=DTL_CACHE2;phase++){
  if(phase!=DTL_MAKE){assert(dtl_cpu_ack(l,k,phase,0));assert(dtl_cpu_ack(l,k,phase,1));}
  assert(dtl_advance(l,k,phase));
 }
 for(unsigned r=0;r<ROOTS;r++){
  assert(dtl_publish_root(l,k,r));
  for(unsigned cpu=0;cpu<2;cpu++)if(l->inventory.root[r].residency&(1u<<cpu))assert(dtl_root_ack(l,k,r,cpu));
 }
 assert(dtl_advance(l,k,DTL_PUBLISH));
 assert(!dtl_advance(l,k,DTL_STAMP));
 assert(dtl_prepare_token(l,k,&t[0],nonce[0]));assert(!dtl_advance(l,k,DTL_STAMP));
 assert(dtl_prepare_token(l,k,&t[1],nonce[1]));assert(dtl_advance(l,k,DTL_STAMP));
 for(unsigned i=0;i<2;i++)assert(dtl_admit(l,&t[i],i,i+2,nonce[i],l->epoch,dest,true,false));
}
int main(void){
 const uint64_t target=0x400000;
 struct domain_typed_root_binding roots[ROOTS];
 for(unsigned r=0;r<ROOTS;r++){
  roots[r]=(struct domain_typed_root_binding){r+1,7,root_hpa(r)|0x1e,r==4?0:r==5?3:1u<<(r&1)};
  tables[r][0][r]=root_hpa(r)+0x1000+7;
  tables[r][1][0]=root_hpa(r)+0x2000+7;
  tables[r][2][0]=root_hpa(r)+0x3000+7;
  if(r<4)tables[r][3][15]=target|0x73;
  if(r==1)tables[r][3][31]=target|0x71;
 }
 const struct domain_typed_request q={roots,ROOTS,48,target,24};
 assert(domain_typed_inventory_audit(&q,resolve,0,&workspace,&audit)==DOMAIN_TYPED_OK);
 assert(audit.alias_count==5&&audit.total_nodes==24&&audit.roots[5].aliases==0);
 struct dtl_inventory in={.domain=0x5333,.incarnation=1,.epoch=1,.participants=3,
  .host={target,0x500000,UINT64_C(0x8000000000400003),0},
  .roots=ROOTS,.aliases=audit.alias_count,.refills=ROOTS,.complete=true,.dedicated_data_wb=true};
 for(unsigned r=0;r<ROOTS;r++){
  const struct domain_typed_root_inventory*a=&audit.roots[r];
  in.root[r]=(struct dtl_root){.id=a->binding.id,.generation=a->binding.generation,
   .hpa=a->binding.eptp&~UINT64_C(4095),.owner=r&1,.residency=a->binding.historical_mask,
   .scan_digest=a->scan_digest,.kind=r<2?0:r<4?1:r==4?2:3,.tables_wb=true,.scanned_aliases=a->aliases};
  in.refill[r]=(struct dtl_refill){r+1,9,r+1,r&1,7};
 }
 for(unsigned n=0;n<audit.alias_count;n++){
  const struct domain_typed_alias*a=&audit.aliases[n];
  in.alias[n]=(struct dtl_alias){.root_id=roots[a->root_index].id,.root_generation=7,
   .gpa=a->gpa,.hpa=a->hpa,.raw=a->raw,.type=a->memory_type,.perms=a->raw&7,
   .ipat=a->ignore_pat,.effective_perms=a->permissions};
 }
 struct domain_typed_lease l;
 assert(dtl_init(&l,&in));
 struct dtl_inventory omitted=in;omitted.aliases--;assert(!dtl_init(&l,&omitted));
 assert(dtl_init(&l,&in));struct dtl_token t[2]={{0}};uint64_t nonce[2];
 for(unsigned i=0;i<2;i++)assert(dtl_acquire(&l,i,i+2,1,6,true,false,&t[i],&nonce[i]));
 transition(&l,t,nonce,0);assert(!dtl_release_ready(&l,63,63,3));
 transition(&l,t,nonce,6);
 for(unsigned r=0;r<ROOTS;r++)assert(dtl_retire_root(&l,r,true,true,true));
 for(unsigned i=0;i<2;i++)assert(dtl_release(&l,&t[i],i,nonce[i],63,63));
 assert(dtl_release_ready(&l,63,63,3));
 puts("PASS actual full-root audit -> typed inventory -> retained pins WB/UC/WB policy; no hardware actions");
}
