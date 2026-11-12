/* N47A compiled in the production atomic bridge to share exact predicates. */
static bool sr_domain_eq(struct nested_domain_maintenance_ticket a,struct nested_domain_maintenance_ticket b){return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.epoch==b.epoch&&a.purpose==b.purpose;}
static bool sr_ticket_eq(struct tsr_ticket a,struct tsr_ticket b){return a.nonce&&a.nonce==b.nonce&&catalog_equal(a.source,b.source)&&tam_stamp_eq(a.stamp,b.stamp)&&sr_domain_eq(a.domain,b.domain);}
static void sr_lock(struct tr_source_replace *m){lock(m->owners[0]);}
static void sr_unlock(struct tr_source_replace *m){unlock(m->owners[0]);}
static bool sr_open(enum tsr_state s){return s==TSR_OPEN||s==TSR_RELEASED;}
static bool sr_valid(struct tr_source_replace *m,struct tsr_ticket t){return sr_ticket_eq(m->ticket,t)&&m->state!=TSR_POISONED&&m->state!=TSR_RELEASED&&m->state!=TSR_OPEN&&m->state!=TSR_FRESH;}
static void sr_poison_locked(struct tr_source_replace *m){
 m->state=TSR_POISONED;
 /* Owner-local registry fields are read without the metadata lock by their
  * single writer. Do not race those fields from a remote abort. Shared bridge
  * health and replacement state are checked under the common metadata lock. */
 for(unsigned c=0;c<2;c++)m->owners[c]->bridge->directory.faulted=true;
 trl_poison(m->owners[0]->bridge->lease);
}
bool tsr_abort(struct tr_source_replace *m,struct tsr_ticket t){
 if(!m||!m->owners[0])return false;
 sr_lock(m);bool ok=sr_valid(m,t);if(ok)sr_poison_locked(m);struct ad_updater_gate *next=m->new_gate;sr_unlock(m);
 if(ok){ad_updater_gate_poison(m->old_gate);if(next)ad_updater_gate_poison(next);}return ok;
}
static bool sr_fail(struct tr_source_replace *m){
 sr_lock(m);if(m->state!=TSR_RELEASED)sr_poison_locked(m);struct ad_updater_gate *next=m->new_gate;sr_unlock(m);
 ad_updater_gate_poison(m->old_gate);if(next)ad_updater_gate_poison(next);return false;
}
bool tsr_released(struct tr_source_replace *m,struct tsr_ticket t){
 if(!m||!m->owners[0])return false;
 sr_lock(m);bool ok=m->state==TSR_RELEASED&&sr_ticket_eq(m->ticket,t)&&shared_healthy_locked(m->owners[0])&&shared_healthy_locked(m->owners[1]);sr_unlock(m);return ok;
}
bool tsr_consumer_stopped(struct tr_atomic_owner *o){
 if(!o||!o->replacement)return false;
 struct tr_source_replace *m=o->replacement;
 sr_lock(m);enum tsr_state state=m->state;struct tsr_ticket t=m->ticket;sr_unlock(m);
 if(sr_open(state)||state==TSR_STOPPING)return o->config.consumer_stopped(o->config.context);
 /* Complete expected ticket is already durable before physical close. Query
  * OPEN first, then exact maintenance: a concurrent OPEN->MAINTENANCE switch
  * leaves no unroutable interval. Neither route admits ordinary new work. */
 if(state==TSR_BOUND)return o->config.consumer_stopped(o->config.context)||m->hooks.stopped(m->context,(unsigned)o->bridge->directory.owner,t);
 return state>=TSR_DRAINING&&state<TSR_RELEASED&&m->hooks.stopped(m->context,(unsigned)o->bridge->directory.owner,t);
}
static bool sr_stopped(struct tr_source_replace *m){for(unsigned c=0;c<2;c++)if(!m->hooks.stopped(m->context,c,m->ticket))return false;return true;}
static bool sr_quiet(struct tr_atomic_owner *o){return healthy_locked(o)&&!o->fault.active&&!o->fault.admitted&&!o->fault.permit.active&&!o->entry_active&&!o->entry_operation.live&&!o->bridge->directory.busy&&!o->bridge->refill_operation;}
static bool sr_root_eq(struct tsr_root a,struct tsr_root b){return a.slot==b.slot&&identity_equal(a.identity,b.identity)&&root_equal(a.source,b.source)&&a.key==b.key&&a.eptp==b.eptp&&a.hpa==b.hpa&&a.generation==b.generation&&a.history==b.history&&a.pins==b.pins;}
static bool sr_snapshot(struct tr_atomic_owner *o,struct nested_ept_context_item *item,struct tsr_root *r){
 struct tr_atomic_binding *a=binding(o,item->transaction_token);
 if(!pin_identity(o,a)||!slot_locked(o,a,item)||item->tree.poisoned||item->pins!=a->slot->directory.record.pins||a->slot->directory.record.state!=TD_LIVE||a->slot->typed.state!=TRL_LIVE||(a->slot->directory.record.history&~(UINT64_C(1)<<o->bridge->directory.owner)))return false;
 *r=(struct tsr_root){a->slot,a->slot->directory.identity,a->root,item->key,item->tree.eptp,a->slot->directory.record.hpa,item->tree.generation,a->slot->directory.record.history,item->pins};return true;
}
/* Identity is checked before dereferencing the current, not saved, item. */
static bool sr_live(struct tr_atomic_owner *o,const struct tsr_root *r,bool check_pins){
 if(!r->slot||!identity_equal(r->slot->directory.identity,r->identity)||r->slot->directory.forgotten||!r->slot->directory.item)return false;
 struct tsr_root now;if(!sr_snapshot(o,r->slot->directory.item,&now))return false;
 if(!check_pins)now.pins=r->pins;
 return sr_root_eq(*r,now);
}
static bool sr_receipt(struct tsr_receipt a,struct tr_source_replace *m,unsigned c,struct tsr_root r){return a.success&&a.owner==c&&sr_ticket_eq(a.ticket,m->ticket)&&sr_root_eq(a.root,r);}
static bool sr_mirror(struct tr_atomic_owner *o,struct tr_atomic_config *c){return !c||(c->catalog==o->config.catalog&&c->gate==o->config.gate&&c->authority_context==o->config.authority_context&&catalog_equal(c->catalog_identity,o->config.catalog_identity)&&tam_stamp_eq(c->stamp,o->config.stamp));}
bool tsr_init(struct tr_source_replace *m,struct tr_atomic_owner *a,struct tr_atomic_owner *b,
 struct tr_local_provider *pa,struct tr_local_provider *pb,struct tr_atomic_config *ma,struct tr_atomic_config *mb,const struct tsr_hooks *h,void *ctx){
 if(!m||m->state!=TSR_FRESH||!a||!b||a==b||!a->initialized||!b->initialized||a->maintenance||b->maintenance||a->replacement||b->replacement||!pa||!pb||pa->bridge!=a->bridge||pb->bridge!=b->bridge||!pa->serial||pa->serial!=pb->serial||!pa->completed||!pb->completed||pa->slots!=a->config.slots||pb->slots!=b->config.slots||pa->slot_count!=a->config.slot_count||pb->slot_count!=b->config.slot_count||pa->owner!=0||pb->owner!=1||
 !h||!h->plan_close||!h->close||!h->stopped||!h->park||!h->restore||!h->complete||a->config.lock!=b->config.lock||a->config.unlock!=b->config.unlock||a->bridge->directory.owner!=0||b->bridge->directory.owner!=1||a->bridge->lease!=b->bridge->lease||a->bridge->directory.directory!=b->bridge->directory.directory||a->bridge->directory.directory->participants!=3||
 a->config.catalog!=b->config.catalog||a->config.gate!=b->config.gate||a->config.catalog->atomic_uc||a->config.catalog->atomic_2m||a->config.catalog->atomic_1g||!a->config.catalog->atomic_wb||!tam_stamp_eq(a->config.stamp,b->config.stamp)||!sr_quiet(a)||!sr_quiet(b)||!sr_mirror(a,ma)||!sr_mirror(b,mb))return false;
 volatile unsigned char *z=(void *)m;for(size_t i=0;i<sizeof *m;i++)z[i]=0;
 m->owners[0]=a;m->owners[1]=b;m->providers[0]=pa;m->providers[1]=pb;m->mirrors[0]=ma;m->mirrors[1]=mb;m->hooks=*h;m->context=ctx;m->old_catalog=a->config.catalog;m->old_gate=a->config.gate;m->type_epoch=a->bridge->lease->type_epoch;m->state=TSR_OPEN;
 for(unsigned c=0;c<2;c++){struct nested_ept_contexts *r=m->owners[c]->registry;m->registry_config[c]=*r;m->registry_config[c].items=m->registry_config[c].active=NULL;m->registry_config[c].count=0;m->owners[c]->replacement=m;}return true;
}
/* Conservative headroom for the bounded retirement + two new roots. This is
 * resource admission, not an exact accounting of platform work. */
static bool sr_budget_locked(struct tr_source_replace *m){
 struct typed_directory *d=m->owners[0]->bridge->directory.directory;struct typed_root_lease *l=m->owners[0]->bridge->lease;
 if(d->closed||d->sealed||UINT64_MAX-d->version<256||UINT64_MAX-d->next_id<2||UINT64_MAX-d->next_nonce<64||UINT64_MAX-l->version<64||UINT64_MAX-l->next_nonce<2||l->reserved>l->budget||l->budget-l->reserved<2||m->next_command>UINT64_MAX-64)return false;
 for(size_t n=0;n<m->old_catalog->root_count;n++)if(m->old_catalog->roots[n].generation==UINT64_MAX)return false;
 /* An admitted operation can write registry/trace/binding fields outside
  * this lock. Defer those owner-local scans until its td_end synchronizes. */
 if(d->operations||d->ops)return true;
 for(unsigned c=0;c<2;c++){struct tr_atomic_owner *o=m->owners[c];size_t slots=0,traces=0;
  if(o->registry->count>TSR_ROOTS||o->registry->tick==UINT64_MAX||*m->providers[c]->serial>UINT64_MAX-2*TSR_ROOTS)return false;
  for(size_t n=0;n<o->config.slot_count;n++)if(!o->config.bindings[n].bound&&!o->config.bindings[n].pin.live&&!o->config.slots[n].directory.reserved)slots++;
  for(size_t n=0;n<o->config.trace_count;n++)if(!o->config.traces[n].used)traces++;
  if(!slots||!traces)return false;
 }return true;
}
static bool sr_successor_inventory(struct nested_root_catalog *a,struct nested_root_catalog *b){
 if(b->predecessor!=a||a->span_count!=b->span_count||a->root_count!=b->root_count||a->maxphys!=b->maxphys||b->next_nonce<a->next_nonce)return false;
 for(size_t n=0;n<a->span_count;n++){const struct nrc_span *x=&a->spans[n],*y=&b->spans[n];
  if(!catalog_equal(y->identity,b->identity)||x->object!=y->object||x->lifetime!=y->lifetime||x->gpa!=y->gpa||x->hpa!=y->hpa||x->bytes!=y->bytes||x->table_view!=y->table_view||x->permissions!=y->permissions||x->allowed_types!=y->allowed_types||x->role!=y->role||x->ignore_pat!=y->ignore_pat)return false;
 }
 for(size_t n=0;n<a->root_count;n++){struct nrc_root r=a->roots[n];if(r.generation==UINT64_MAX)return false;r.identity=b->identity;r.generation++;if(!root_equal(r,b->roots[n]))return false;}
 return true;
}
static bool sr_close_health_locked(struct tr_source_replace *m){
 struct typed_directory *d=m->owners[0]->bridge->directory.directory;
 return shared_healthy_locked(m->owners[0])&&shared_healthy_locked(m->owners[1])&&
  ((d->operations||d->ops)||(!m->owners[0]->registry->poisoned&&!m->owners[1]->registry->poisoned));
}
bool tsr_close(struct tr_source_replace *m,struct tsr_ticket *out){
 if(!m||!out||!m->owners[0])return false;
 sr_lock(m);bool ok=m->state==TSR_OPEN&&m->next_nonce!=UINT64_MAX&&sr_budget_locked(m)&&sr_close_health_locked(m);
 if(ok){m->ticket=(struct tsr_ticket){.source=m->old_catalog->identity,.stamp=m->owners[0]->config.stamp,.nonce=++m->next_nonce};m->state=TSR_STOPPING;}sr_unlock(m);if(!ok)return false;
 struct nested_domain_maintenance_ticket domain={0};
 if(!m->hooks.plan_close(m->context,m->ticket,&domain))return sr_fail(m);
 sr_lock(m);ok=m->state==TSR_STOPPING&&domain.domain_id==m->ticket.source.domain&&domain.incarnation==m->ticket.source.incarnation&&domain.epoch&&domain.purpose==NESTED_DOMAIN_SOURCE_REPLACE;
 if(ok){m->ticket.domain=domain;m->state=TSR_BOUND;*out=m->ticket;}sr_unlock(m);
 if(!ok||!m->hooks.close(m->context,m->ticket)||!sr_stopped(m))return sr_fail(m);
 sr_lock(m);ok=sr_valid(m,*out)&&m->state==TSR_BOUND&&sr_close_health_locked(m);if(ok)m->state=TSR_DRAINING;sr_unlock(m);
 return ok?true:sr_fail(m);
}
enum tsr_drain tsr_drain(struct tr_source_replace *m,struct tsr_ticket t){
 if(!m||!m->owners[0])return TSR_DRAIN_FAILED;
 sr_lock(m);bool valid=sr_valid(m,t)&&(m->state==TSR_DRAINING||m->state==TSR_UPDATER_DRAIN),wait=false,close=false,bad=false;
 if(valid){if(++m->polls>TSR_DRAIN_POLLS)bad=true;else if(m->state==TSR_DRAINING){struct typed_directory *d=m->owners[0]->bridge->directory.directory;wait=d->operations||d->ops;
 if(!wait){if(!sr_quiet(m->owners[0])||!sr_quiet(m->owners[1])||!sr_budget_locked(m))bad=true;else{m->state=TSR_UPDATER_DRAIN;close=true;}}}}
 sr_unlock(m);if(!valid)return TSR_DRAIN_FAILED;
 if(bad||!sr_stopped(m)){sr_fail(m);return TSR_DRAIN_FAILED;}if(wait)return TSR_DRAIN_WAIT;
 if(close&&!ad_updater_gate_close(m->old_gate,t.stamp)){sr_fail(m);return TSR_DRAIN_FAILED;}
 if(!ad_updater_gate_drained(m->old_gate,t.stamp)){if(__atomic_load_n(&m->old_gate->fault,__ATOMIC_ACQUIRE)){sr_fail(m);return TSR_DRAIN_FAILED;}return TSR_DRAIN_WAIT;}
 if(!nrc_close(m->old_catalog,t.source)){sr_fail(m);return TSR_DRAIN_FAILED;}
 sr_lock(m);valid=sr_valid(m,t)&&m->state==TSR_UPDATER_DRAIN;if(valid)m->state=TSR_CLOSED;sr_unlock(m);return valid?TSR_DRAIN_READY:TSR_DRAIN_FAILED;
}
/* Armed command is consumed BEFORE any stopped/platform callback. Keep its
 * exact key/identity until transaction end; no phase-only permission exists. */
static bool sr_command_take(struct tr_atomic_owner *o,enum nested_context_operation kind){
 struct tr_source_replace *m=o->replacement;if(!m)return true;unsigned c=(unsigned)o->bridge->directory.owner;
 sr_lock(m);if(sr_open(m->state)){sr_unlock(m);return true;}
 struct tsr_command *q=&m->command[c];bool ok=m->state!=TSR_POISONED&&m->busy[c]&&q->armed&&!q->consumed&&!q->admitted&&q->nonce&&q->kind==kind;
 if(ok&&(kind==NESTED_CONTEXT_UNPIN||kind==NESTED_CONTEXT_PIN))ok=sr_live(o,&q->root,true)&&(kind!=NESTED_CONTEXT_PIN||o->registry->active==q->root.slot->directory.item);
 if(ok&&kind==NESTED_CONTEXT_DISPOSE){
  ok=m->state==TSR_CLOSED&&o->registry->count==m->count[c];
  for(size_t n=0;n<m->count[c]&&ok;n++)ok=sr_live(o,&m->old[c][n],false)&&!m->old[c][n].slot->directory.item->pins;
 }
 if(ok){q->armed=false;q->consumed=true;}sr_unlock(m);return ok;
}
static bool sr_command_admit(struct tr_atomic_owner *o){struct tr_source_replace *m=o->replacement;if(!m||sr_open(m->state))return true;struct tsr_command *q=&m->command[o->bridge->directory.owner];bool ok=m->state!=TSR_POISONED&&m->busy[o->bridge->directory.owner]&&q->consumed&&!q->admitted;if(ok)q->admitted=true;return ok;}
static bool sr_command_key(struct tr_atomic_owner *o,uint64_t key){struct tr_source_replace *m=o->replacement;if(!m)return true;sr_lock(m);bool ok=sr_open(m->state)||(m->state==TSR_DRAINING&&o->bridge->directory.busy&&!m->command[o->bridge->directory.owner].admitted)||(m->state==TSR_PUBLISHED&&m->command[o->bridge->directory.owner].admitted&&m->command[o->bridge->directory.owner].kind==NESTED_CONTEXT_SELECT&&m->command[o->bridge->directory.owner].key==key);sr_unlock(m);return ok;}
static bool sr_arm(struct tr_source_replace *m,unsigned c,enum nested_context_operation kind,const struct tsr_root *r,uint64_t key){
 sr_lock(m);struct tsr_command *q=&m->command[c];bool ok=m->state!=TSR_POISONED&&m->busy[c]&&!q->armed&&!q->consumed&&!q->admitted&&m->next_command!=UINT64_MAX;
 if(ok)*q=(struct tsr_command){.nonce=++m->next_command,.key=key,.kind=kind,.root=r?*r:(struct tsr_root){0},.armed=true};
 sr_unlock(m);return ok;
}
static bool sr_command_done(struct tr_source_replace *m,unsigned c){sr_lock(m);struct tsr_command *q=&m->command[c];struct tr_atomic_owner *o=m->owners[c];bool ok=m->state!=TSR_POISONED&&q->consumed&&q->admitted&&!q->armed&&!o->bridge->directory.busy&&!o->bridge->directory.operation.live;
 if(ok&&(q->kind==NESTED_CONTEXT_PIN||q->kind==NESTED_CONTEXT_UNPIN)){uint32_t expect=q->kind==NESTED_CONTEXT_PIN?q->root.pins+1:q->root.pins-1;ok=sr_live(o,&q->root,false)&&q->root.slot->directory.item->pins==expect;}
 *q=(struct tsr_command){0};sr_unlock(m);return ok;}
static bool sr_owner_start(struct tr_source_replace *m,unsigned c,struct tsr_ticket t,enum tsr_state state){
 if(!m||c>=2||!m->owners[0]||m->owners[c]->config.current_cpu(m->owners[c]->config.context)!=c)return false;
 sr_lock(m);bool ok=sr_valid(m,t)&&m->state==state&&!((state==TSR_CLOSED?m->retired:m->prepared)&(UINT64_C(1)<<c))&&!m->busy[c]&&sr_quiet(m->owners[c]);if(ok)m->busy[c]=true;sr_unlock(m);return ok;
}
static bool sr_old_proof(struct tr_source_replace *m,unsigned c){struct tr_atomic_owner *o=m->owners[c];
 for(size_t n=0;n<m->count[c];n++){struct tsr_root *r=&m->old[c][n];struct tr_atomic_binding *a=binding(o,&r->slot->directory);if(!identity_equal(r->slot->directory.identity,r->identity)||!a||!a->released||a->pin.live||!root_equal(a->root,r->source)||!tr_local_completed_matches(m->providers[c],r->slot))return false;}
 for(size_t n=0;n<o->config.trace_count;n++)if(o->config.traces[n].used&&(o->config.traces[n].usable||o->config.traces[n].registered))return false;
 return true;
}
bool tsr_retire_owner(struct tr_source_replace *m,unsigned c,struct tsr_ticket t){
 if(!sr_owner_start(m,c,t,TSR_CLOSED))return false;
 struct tr_atomic_owner *o=m->owners[c];if(!m->hooks.stopped(m->context,c,t))return sr_fail(m);
 sr_lock(m);size_t count=0;bool ok=true;for(struct nested_ept_context_item *p=o->registry->items;p&&ok;p=p->next){ok=count<TSR_ROOTS&&sr_snapshot(o,p,&m->old[c][count]);if(ok)count++;}
 if(ok)ok=count&&count==o->registry->count&&o->registry->active;
 if(ok){m->count[c]=count;m->active_key[c]=o->registry->active->key;m->registry_config[c].tick=o->registry->tick;}
 for(struct tr_registry_slot *s=o->bridge->slots;s&&ok;s=s->next)if(!s->directory.forgotten){bool found=false;for(size_t n=0;n<count;n++)if(m->old[c][n].slot==s)found=true;if(!found)ok=false;}
 sr_unlock(m);if(!ok)return sr_fail(m);
 struct tsr_root active_root={0};for(size_t n=0;n<count;n++)if(m->old[c][n].key==m->active_key[c])active_root=m->old[c][n];
 struct tsr_receipt parked={0};if(!m->hooks.park(m->context,c,t,m->old[c],count,&parked)||!sr_receipt(parked,m,c,active_root)||!m->hooks.stopped(m->context,c,t))return sr_fail(m);
 sr_lock(m);ok=sr_valid(m,t)&&m->state==TSR_CLOSED;for(size_t n=0;n<count&&ok;n++)ok=sr_live(o,&m->old[c][n],true);if(ok)m->park[c]=parked;sr_unlock(m);if(!ok)return sr_fail(m);
 for(size_t n=0;n<count;n++)for(unsigned p=0;p<m->old[c][n].pins;p++){
  struct tsr_root r=m->old[c][n];r.pins-=p;
  if(!sr_arm(m,c,NESTED_CONTEXT_UNPIN,&r,0)||!nested_ept_contexts_unpin(o->registry,r.slot->directory.item)||!sr_command_done(m,c))return sr_fail(m);
 }
 if(!sr_arm(m,c,NESTED_CONTEXT_DISPOSE,NULL,0)||!nested_ept_contexts_destroy(o->registry)||!sr_command_done(m,c))return sr_fail(m);
 sr_lock(m);ok=sr_valid(m,t)&&m->state==TSR_CLOSED&&sr_old_proof(m,c)&&!o->registry->initialized&&!o->registry->items&&!o->registry->active&&!o->registry->count;if(ok){m->retired|=UINT64_C(1)<<c;m->busy[c]=false;if(m->retired==3)m->state=TSR_RETIRED;}sr_unlock(m);return ok?true:sr_fail(m);
}
bool tsr_replace(struct tr_source_replace *m,struct tsr_ticket t,tsr_change_fn change,void *ctx){
 if(!m||!m->owners[0]||!change)return false;
 sr_lock(m);bool ok=sr_valid(m,t)&&m->state==TSR_RETIRED&&m->retired==3&&!m->busy[0]&&!m->busy[1]&&sr_old_proof(m,0)&&sr_old_proof(m,1);if(ok)m->state=TSR_CHANGING;sr_unlock(m);if(!ok)return false;
 if(!sr_stopped(m)||!nrc_drained(m->old_catalog,t.source)||!ad_updater_gate_drained(m->old_gate,t.stamp))return sr_fail(m);
 struct nested_root_catalog *catalog=NULL;struct ad_updater_gate *gate=NULL;void *authority_context[2]={NULL,NULL};
 if(!change(ctx,t,m->old_catalog,m->old_gate,&catalog,&gate,authority_context))return sr_fail(m);
 if(!catalog||!gate||gate->predecessor!=m->old_gate||gate->next_nonce<m->old_gate->next_nonce||!sr_successor_inventory(m->old_catalog,catalog)||catalog==m->old_catalog||gate==m->old_gate||!m->old_catalog->superseded||!m->old_gate->superseded||!catalog->published||catalog->closed||catalog->atomic_uc||catalog->atomic_2m||catalog->atomic_1g||!catalog->atomic_wb||catalog_equal(catalog->identity,t.source)||catalog->identity.domain!=t.source.domain||catalog->identity.incarnation!=t.source.incarnation||gate->stamp.domain_id!=catalog->identity.domain||gate->stamp.incarnation!=catalog->identity.incarnation||gate->stamp.context_id!=catalog->identity.catalog||gate->stamp.lifetime!=catalog->identity.lifetime||!ad_updater_gate_idle(gate,gate->stamp)||!nrc_idle(catalog,catalog->identity)||!sr_stopped(m))return sr_fail(m);
 sr_lock(m);ok=sr_valid(m,t)&&m->state==TSR_CHANGING&&sr_old_proof(m,0)&&sr_old_proof(m,1)&&m->owners[0]->bridge->lease->type_epoch==m->type_epoch;
 if(ok){m->new_catalog=catalog;m->new_gate=gate;m->new_stamp=gate->stamp;for(unsigned c=0;c<2;c++){struct tr_atomic_owner *o=m->owners[c];o->config.authority_context=authority_context[c];o->config.catalog=catalog;o->config.catalog_identity=catalog->identity;o->config.gate=gate;o->config.stamp=gate->stamp;
 if(m->mirrors[c]){m->mirrors[c]->authority_context=authority_context[c];m->mirrors[c]->catalog=catalog;m->mirrors[c]->catalog_identity=catalog->identity;m->mirrors[c]->gate=gate;m->mirrors[c]->stamp=gate->stamp;}
 /* Sole rearm of a fully destroyed registry, preserving monotonic LRU tick
  * and all hooks. No owner, bridge, slot, trace, provider or lease reset. */
 *o->registry=m->registry_config[c];}
 m->state=TSR_PUBLISHED;}sr_unlock(m);return ok?true:sr_fail(m);
}
bool tsr_prepare_owner(struct tr_source_replace *m,unsigned c,struct tsr_ticket t){
 if(!sr_owner_start(m,c,t,TSR_PUBLISHED))return false;
 struct tr_atomic_owner *o=m->owners[c];if(!m->hooks.stopped(m->context,c,t))return sr_fail(m);
 if(!sr_arm(m,c,NESTED_CONTEXT_SELECT,NULL,m->active_key[c])||nested_ept_contexts_select(o->registry,m->active_key[c])!=NESTED_CONTEXT_OK||!sr_command_done(m,c))return sr_fail(m);
 sr_lock(m);bool ok=sr_valid(m,t)&&m->state==TSR_PUBLISHED&&sr_snapshot(o,o->registry->active,&m->fresh[c])&&!m->fresh[c].pins;sr_unlock(m);if(!ok)return sr_fail(m);
 if(!sr_arm(m,c,NESTED_CONTEXT_PIN,&m->fresh[c],0)||!nested_ept_contexts_pin_active(o->registry)||!sr_command_done(m,c))return sr_fail(m);
 sr_lock(m);ok=sr_snapshot(o,o->registry->active,&m->fresh[c]);sr_unlock(m);if(!ok)return sr_fail(m);
 struct tsr_receipt receipt={0};if(!m->hooks.restore(m->context,c,t,&m->fresh[c],&receipt)||!sr_receipt(receipt,m,c,m->fresh[c])||!m->hooks.stopped(m->context,c,t))return sr_fail(m);
 sr_lock(m);ok=sr_valid(m,t)&&m->state==TSR_PUBLISHED&&sr_live(o,&m->fresh[c],true)&&sr_mirror(o,m->mirrors[c]);if(ok){m->restore[c]=receipt;m->prepared|=UINT64_C(1)<<c;m->busy[c]=false;}sr_unlock(m);return ok?true:sr_fail(m);
}
/* No directory lock spans either bank lock. All owner preparation has ended
 * and ordinary source/updater admission remains shut by the outer gate. */
static bool sr_sources_ready(struct tr_source_replace *m){
 if(!m->new_catalog||!m->new_gate||!ad_updater_gate_idle(m->new_gate,m->new_stamp))return false;
 for(unsigned c=0;c<2;c++){struct tr_atomic_owner *o=m->owners[c];struct tsr_root *r=&m->fresh[c];
  if(o->config.catalog!=m->new_catalog||o->config.gate!=m->new_gate||!tam_stamp_eq(o->config.stamp,m->new_stamp)||!source_guard(o,binding(o,&r->slot->directory)))return false;
 }return true;
}
bool tsr_finish(struct tr_source_replace *m,struct tsr_ticket t){
 if(!m||!m->owners[0])return false;
 sr_lock(m);bool ok=sr_valid(m,t)&&m->state==TSR_PUBLISHED&&m->retired==3&&m->prepared==3&&!m->busy[0]&&!m->busy[1]&&m->owners[0]->bridge->lease->type_epoch==m->type_epoch;
 for(unsigned c=0;c<2&&ok;c++)ok=sr_quiet(m->owners[c])&&sr_old_proof(m,c)&&sr_live(m->owners[c],&m->fresh[c],true)&&sr_mirror(m->owners[c],m->mirrors[c])&&sr_receipt(m->restore[c],m,c,m->fresh[c]);
 if(ok)m->state=TSR_COMMITTING;
 sr_unlock(m);if(!ok)return false;
 if(!sr_stopped(m)||!nrc_drained(m->old_catalog,t.source)||!ad_updater_gate_drained(m->old_gate,t.stamp)||!sr_sources_ready(m)||!m->hooks.complete(m->context,t)||!sr_sources_ready(m))return sr_fail(m);
 sr_lock(m);ok=sr_valid(m,t)&&m->state==TSR_COMMITTING&&healthy_locked(m->owners[0])&&healthy_locked(m->owners[1]);if(ok)m->state=TSR_RELEASED;sr_unlock(m);return ok?true:sr_fail(m);
}
