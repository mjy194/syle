#include "domain_typed_lease.h"
static uint64_t mask(unsigned n){return (UINT64_C(1)<<n)-1;}
static bool page(uint64_t h){return h&&!(h&4095)&&h<(UINT64_C(1)<<48);}
static int root(const struct dtl_inventory*i,uint64_t id){for(unsigned r=0;r<i->roots;r++)if(i->root[r].id==id)return (int)r;return -1;}
bool dtl_init_type(struct domain_typed_lease*l,const struct dtl_inventory*i,unsigned nonwb_type){
 if((nonwb_type!=0&&nonwb_type!=4)||!l||!i||!i->domain||!i->incarnation||!i->epoch||!i->participants||!i->complete||!i->dedicated_data_wb||
 !i->roots||i->roots>DTL_ROOTS||i->aliases>DTL_ALIASES||!i->refills||i->refills>DTL_REFILLS||
 !page(i->host.target)||!page(i->host.table)||i->host.target==i->host.table||i->host.index>=512||
 (i->host.oldraw&UINT64_C(0x000ffffffffff000))!=i->host.target||
 (i->host.oldraw&UINT64_C(0x800000000000009b))!=UINT64_C(0x8000000000000003))return false;
 for(unsigned r=0;r<i->roots;r++){
  const struct dtl_root*p=&i->root[r];
  if(!p->id||!p->generation||!page(p->hpa)||p->hpa==i->host.target||!p->scan_digest||!p->tables_wb||
   p->owner>=64||!(i->participants&(1ull<<p->owner))||p->kind>3||(p->residency&~i->participants))return false;
  for(unsigned j=0;j<r;j++)if(i->root[j].id==p->id||i->root[j].hpa==p->hpa)return false;
 }
 for(unsigned a=0;a<i->aliases;a++){
  const struct dtl_alias*p=&i->alias[a];int r=root(i,p->root_id);
  if(r<0||p->root_generation!=i->root[r].generation||(p->gpa&4095)||p->gpa>=(UINT64_C(1)<<48)||p->hpa!=i->host.target||
   p->type!=6||!p->ipat||(p->perms&4)||!p->perms||p->perms>7||((p->perms&2)&&!(p->perms&1))||!(p->perms&1)||
   !p->effective_perms||!(p->effective_perms&1)||(p->effective_perms&~p->perms)||(p->effective_perms&4)||
   p->raw!=(p->hpa|UINT64_C(0x70)|p->perms))return false;
  unsigned count=1;for(unsigned b=0;b<a;b++)if(i->alias[b].root_id==p->root_id){if(i->alias[b].gpa==p->gpa)return false;count++;}
  if(count>8)return false;
 }
 for(unsigned r=0;r<i->roots;r++){
  unsigned count=0;for(unsigned a=0;a<i->aliases;a++)if(i->alias[a].root_id==i->root[r].id)count++;
  if(count!=i->root[r].scanned_aliases)return false;
 }
 for(unsigned f=0;f<i->refills;f++){
  const struct dtl_refill*p=&i->refill[f];int r=root(i,p->root_id);
  if(!p->id||!p->generation||r<0||p->owner!=i->root[r].owner||p->root_generation!=i->root[r].generation)return false;
  for(unsigned j=0;j<f;j++)if(i->refill[j].id==p->id)return false;
 }
 *l=(struct domain_typed_lease){.inventory=*i,.phase=DTL_OPEN,.type=6,.nonwb_type=nonwb_type,.epoch=i->epoch};return true;
}
static bool member(struct domain_typed_lease*l,struct dtl_token*t,uint64_t n){
 if(!l||!t||!t->live||t->lease!=l||!n||t->nonce!=n)return false;
 for(struct dtl_token*p=l->tokens;p;p=p->next)if(p==t)return true;
 return false;
}
bool dtl_acquire(struct domain_typed_lease*l,uint64_t owner,unsigned root_index,uint64_t epoch,unsigned type,bool ipat,bool ad,struct dtl_token*t,uint64_t*out){
 if(!l||!t||!out||l->phase!=DTL_OPEN||epoch!=l->epoch||type!=l->type||!ipat||ad||t->live||l->nonce==UINT64_MAX||l->pins==UINT64_MAX)return false;
 if(root_index>=l->inventory.roots||l->inventory.root[root_index].owner!=owner||(l->retired&(1ull<<root_index)))return false;
 *t=(struct dtl_token){.lease=l,.next=l->tokens,.owner=owner,.root_index=root_index,.root_generation=l->inventory.root[root_index].generation,.nonce=++l->nonce,.epoch=epoch,.type=type,.live=true};
 l->tokens=t;l->pins++;*out=t->nonce;return true;
}
bool dtl_admit(struct domain_typed_lease*l,struct dtl_token*t,uint64_t owner,unsigned root_index,uint64_t nonce,uint64_t epoch,unsigned type,bool ipat,bool ad){
 return member(l,t,nonce)&&l->phase==DTL_OPEN&&root_index==t->root_index&&root_index<l->inventory.roots&&t->root_generation==l->inventory.root[root_index].generation&&!(l->retired&(1ull<<root_index))&&t->owner==owner&&t->epoch==epoch&&epoch==l->epoch&&t->type==type&&type==l->type&&ipat&&!ad;
}
bool dtl_release(struct domain_typed_lease*l,struct dtl_token*t,uint64_t owner,uint64_t nonce,uint64_t retired,uint64_t revoked){
 if(!member(l,t,nonce)||t->owner!=owner||!(l->retired&(1ull<<t->root_index))||retired!=mask(l->inventory.roots)||revoked!=mask(l->inventory.refills))return false;
 struct dtl_token**p=&l->tokens;while(*p!=t)p=&(*p)->next;*p=t->next;l->pins--;t->live=false;t->lease=0;t->next=0;return true;
}
static bool ticket(struct domain_typed_lease*l,struct dtl_ticket t){return l&&l->phase!=DTL_OPEN&&l->phase!=DTL_POISON&&t.domain==l->inventory.domain&&t.incarnation==l->inventory.incarnation&&t.epoch==l->epoch&&t.source==l->type&&t.destination==l->destination;}
bool dtl_begin(struct domain_typed_lease*l,unsigned dest,uint64_t rooted,uint64_t closed,uint64_t drained,struct dtl_ticket*out){
 if(!l||!out||l->phase!=DTL_OPEN||l->retired||l->epoch==UINT64_MAX||!((l->type==6&&dest==l->nonwb_type)||(l->type==l->nonwb_type&&dest==6))||
 rooted!=l->inventory.participants||closed!=rooted||drained!=rooted)return false;
 l->epoch++;l->destination=dest;l->phase=DTL_DETACH;l->detached=l->revoked=l->published=l->cpu_acks=0;
 for(unsigned r=0;r<DTL_ROOTS;r++)l->root_acks[r]=0;
 for(struct dtl_token*p=l->tokens;p;p=p->next)p->prepared=false;
 *out=(struct dtl_ticket){l->inventory.domain,l->inventory.incarnation,l->epoch,l->type,dest};return true;
}
bool dtl_detach(struct domain_typed_lease*l,struct dtl_ticket t,unsigned r){if(!ticket(l,t)||l->phase!=DTL_DETACH||r>=l->inventory.roots||(l->detached&(1ull<<r)))return false;l->detached|=1ull<<r;return true;}
bool dtl_revoke(struct domain_typed_lease*l,struct dtl_ticket t,unsigned r){if(!ticket(l,t)||l->phase!=DTL_DETACH||r>=l->inventory.refills||(l->revoked&(1ull<<r)))return false;l->revoked|=1ull<<r;return true;}
bool dtl_root_ack(struct domain_typed_lease*l,struct dtl_ticket t,unsigned r,unsigned cpu){
 if(!ticket(l,t)||r>=l->inventory.roots||cpu>=64)return false;
 if(l->phase==DTL_DETACH){if(l->detached!=mask(l->inventory.roots)||l->revoked!=mask(l->inventory.refills))return false;}
 else if(l->phase==DTL_PUBLISH){if(!(l->published&(1ull<<r)))return false;}else return false;
 uint64_t bit=1ull<<cpu;if(!(l->inventory.root[r].residency&bit)||(l->root_acks[r]&bit))return false;l->root_acks[r]|=bit;return true;
}
bool dtl_publish_root(struct domain_typed_lease*l,struct dtl_ticket t,unsigned r){if(!ticket(l,t)||l->phase!=DTL_PUBLISH||r>=l->inventory.roots||(l->published&(1ull<<r)))return false;l->published|=1ull<<r;return true;}
bool dtl_cpu_ack(struct domain_typed_lease*l,struct dtl_ticket t,enum dtl_phase phase,unsigned cpu){
 if(!ticket(l,t)||phase!=l->phase||cpu>=64||(phase!=DTL_TLB1&&phase!=DTL_CACHE1&&phase!=DTL_TLB2&&phase!=DTL_CACHE2))return false;
 uint64_t bit=1ull<<cpu;if(!(l->inventory.participants&bit)||(l->cpu_acks&bit))return false;l->cpu_acks|=bit;return true;
}
bool dtl_prepare_token(struct domain_typed_lease*l,struct dtl_ticket t,struct dtl_token*p,uint64_t nonce){
 if(!ticket(l,t)||l->phase!=DTL_STAMP||!member(l,p,nonce)||p->prepared)return false;
 p->epoch=l->epoch;p->type=l->destination;p->prepared=true;return true;
}
bool dtl_advance(struct domain_typed_lease*l,struct dtl_ticket t,enum dtl_phase phase){
 if(!ticket(l,t)||phase!=l->phase)return false;
 if(phase==DTL_DETACH||phase==DTL_PUBLISH){
  if(phase==DTL_DETACH&&(l->detached!=mask(l->inventory.roots)||l->revoked!=mask(l->inventory.refills)))return false;
  if(phase==DTL_PUBLISH&&l->published!=mask(l->inventory.roots))return false;
  for(unsigned r=0;r<l->inventory.roots;r++)if(l->root_acks[r]!=l->inventory.root[r].residency)return false;
 }else if(phase==DTL_TLB1||phase==DTL_CACHE1||phase==DTL_TLB2||phase==DTL_CACHE2){if(l->cpu_acks!=l->inventory.participants)return false;}
 else if(phase==DTL_STAMP){for(struct dtl_token*p=l->tokens;p;p=p->next)if(!p->prepared)return false;l->type=l->destination;l->phase=DTL_OPEN;return true;}
 else if(phase!=DTL_BREAK&&phase!=DTL_MAKE)return false;
 l->phase++;l->cpu_acks=0;
 if(l->phase==DTL_PUBLISH)for(unsigned r=0;r<DTL_ROOTS;r++)l->root_acks[r]=0;
 return true;
}
void dtl_fail(struct domain_typed_lease*l){if(l)l->phase=DTL_POISON;}
bool dtl_release_ready(const struct domain_typed_lease*l,uint64_t retired,uint64_t flushed,uint64_t parked){return l&&l->phase==DTL_OPEN&&l->type==6&&l->retired==mask(l->inventory.roots)&&!l->pins&&!l->tokens&&retired==mask(l->inventory.roots)&&flushed==retired&&parked==l->inventory.participants;}

bool dtl_retire_root(struct domain_typed_lease*l,unsigned r,bool vmcs,bool refill,bool flush){
 if(!l||l->phase!=DTL_OPEN||l->type!=6||r>=l->inventory.roots||!vmcs||!refill||!flush||(l->retired&(1ull<<r)))return false;
 l->retired|=1ull<<r;return true;
}

bool dtl_init(struct domain_typed_lease *l,const struct dtl_inventory *i) {
    return dtl_init_type(l,i,0);
}

bool dtl_root_residency_extend(struct domain_typed_lease*l,unsigned index,
 uint64_t id,uint64_t generation,uint64_t residency){
 if(!l||l->phase!=DTL_OPEN||index>=l->inventory.roots||
    (l->retired&(UINT64_C(1)<<index))||(residency&~l->inventory.participants))return false;
 struct dtl_root *r=&l->inventory.root[index];
 if(r->id!=id||r->generation!=generation||(r->residency&~residency))return false;
 r->residency=residency;return true;
}
