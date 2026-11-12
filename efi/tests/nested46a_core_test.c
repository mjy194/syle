/* Production fixed UC atomic source/typed owner/TAM, owned test backing.
 * Physical mappings and INVEPT are mocks: no hardware or physical UC claim. */
#include "n46a_fixture_base.inc"
static unsigned auth_bad46;
static bool authority46(void *p,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *out){
 if(!mauthority(p,s,gpa,out))return false;
 out->backing.memory_type=s->allowed_types==NRC_TYPE_UC?0:6;
 if(s->role==NRC_DATA)switch(auth_bad46){
 case 1:out->backing.memory_type^=6;break;
 case 2:out->backing.hpa+=4096;break;
 case 3:out->backing.permissions|=4;break;
 case 4:out->backing.ignore_pat=false;break;
 case 5:out->backing.lifetime++;break;
 case 6:out->atomic_view=tables[0];out->pinned_writable=true;break;
 }
 return true;
}
static void setup46(struct fixture *f,size_t trace_count){
 memset(f,0,sizeof *f);memset(tables,0,sizeof tables);
 f->id=(struct nrc_identity){1,2,3,4};f->stamp=(struct ad_entry_stamp){1,2,3,5,4};
 assert(nrc_init_atomic_uc(&f->catalog,f->id,48,f->spans,11,f->roots,3,authority46,0x200000,DATA,17));
 for(unsigned i=0;i<9;i++){
  struct nrc_span s={.identity=f->id,.object=i+1,.lifetime=50+i,.gpa=gpas[i],.hpa=0x100000+i*4096,
   .bytes=4096,.table_view=tables[i],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,tables[i]));
 }
 for(unsigned i=0;i<2;i++){
  struct nrc_span s={.identity=f->id,.object=20+i,.lifetime=80+i,.gpa=0x200000+i*0x8000,
   .hpa=DATA+i*0x4000,.bytes=4096,.permissions=3,.allowed_types=i?NRC_TYPE_WB:NRC_TYPE_UC,.role=NRC_DATA,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,NULL));
 }
 const unsigned indices[3]={0,1,5};
 for(unsigned i=0;i<3;i++){
  uint64_t root=gpas[indices[i]];
  assert(nrc_add_root(&f->catalog,(struct nrc_root){f->id,30+i,i,10+i,90+i,root|0x5e,root}));
 }
 __atomic_store_n(&tables[0][0],gpas[2]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[1][0],gpas[2]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[2][0],gpas[3]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[3][0],gpas[4]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[4][7],0x200043,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[5][0],gpas[6]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[6][0],gpas[7]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[7][0],gpas[8]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[8][7],0x208073,__ATOMIC_RELAXED);
 assert(nrc_publish(&f->catalog));assert(ad_updater_gate_init(&f->gate,f->stamp));
 assert(td_init(&f->directory,f->id.domain,f->id.incarnation,3,17));
 assert(trl_init_atomic_uc(&f->lease,&f->directory,(struct trl_backing){DATA,99,true},2*SLOTS));
 for(unsigned cpu=0;cpu<2;cpu++){
  struct owner *o=&f->owners[cpu];o->f=f;o->cpu=cpu;o->stopped=true;
  for(unsigned i=0;i<PAGES;i++)o->pages[i]=(struct nested_sparse_page){0x900000+cpu*0x100000+i*4096,memory[cpu][i]};
  assert(nested_owner_pool_init(&o->pool,cpu,100+cpu,o->pages,o->pool_slots,PAGES,48));
  const struct nested_sparse_ops sparse={alloc,release,flush};
  assert(nested_ept_contexts_init_config(&o->registry,&sparse,o,48,2,true,true,NULL,NULL));
  const struct tr_registry_root_provider provider={reserve,evidence};
  const struct tr_atomic_config config={.catalog=&f->catalog,.catalog_identity=f->id,.authority_context=o,
   .gate=&f->gate,.stamp=f->stamp,.slots=o->slots,.bindings=o->bindings,.slot_count=SLOTS,
   .traces=o->traces,.trace_count=trace_count,.lock=lock,.unlock=unlock,
   .current_cpu=current_cpu,.consumer_stopped=stopped,.context=o};
  assert(tr_atomic_init(&o->atomic,&o->bridge,&o->registry,&f->lease,cpu,&provider,o,&config));
  o->provider=(struct tr_local_provider){.bridge=&o->bridge,.slots=o->slots,.completed=o->completed,
   .slot_count=SLOTS,.owner=cpu,.serial=&f->serial,.lock=provider_lock,.unlock=unlock,
   .current_cpu=current_cpu,.healthy=provider_healthy,.invalidate=invalidate,.context=o};
 }
}

static struct maintenance_fixture *make46(void){
 auth_bad46=0;struct maintenance_fixture *m=calloc(1,sizeof *m);assert(m);setup46(&m->f,TRACES);
 const struct tam_hooks hooks={mclose,mstopped,mremove,mflush,mrestore,mopen,mload,mcas};
 assert(tam_init(&m->maintenance,&m->f.owners[0].atomic,&m->f.owners[1].atomic,&hooks,m));return m;
}
static enum tr_atomic_status fault46(struct owner *o,uint64_t gpa,unsigned access,struct nested_ept_result *r){
 o->f->cpu=o->cpu;return tr_atomic_fault(&o->atomic,o->registry.active,gpa,access,r);
}
static uint64_t leaf46(struct owner *o,uint64_t gpa){
 uint64_t h=o->registry.active->tree.eptp&~UINT64_C(4095),raw=0;
 for(unsigned d=0;d<4;d++){
  uint64_t *p=NULL;for(unsigned i=0;i<PAGES;i++)if(o->pool_slots[i].live&&o->pages[i].hpa==h)p=o->pages[i].view;
  assert(p);raw=__atomic_load_n(&p[(gpa>>(39-d*9))&511],__ATOMIC_ACQUIRE);h=raw&~UINT64_C(4095);
 }
 return raw;
}
static void read46(struct owner *o){
 struct nested_ept_result r;assert(fault46(o,0x7000,1,&r)==TR_ATOMIC_FILLED);
 assert(r.status==NESTED_EPT_OK&&r.memory_explicit&&r.ignore_pat&&r.walk_type==6&&r.leaf_type==0&&r.hpa==DATA);
 assert(leaf46(o,0x7000)==(DATA|0x41));
}
static void promote46(struct owner *o){
 struct nested_ept_result r;unsigned old=o->flushes;
 assert(fault46(o,0x7000,2,&r)==TR_ATOMIC_PROMOTED&&o->flushes==old+1&&r.leaf_type==0);
 assert(leaf46(o,0x7000)==(DATA|0x43));
}
static void pin46(struct maintenance_fixture *m,unsigned c){
 struct owner *o=&m->f.owners[c];m->f.cpu=c;
 struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);assert(item);
 m->references[c]=item->tree.eptp;assert(tr_atomic_entry(&o->atomic,item,item->tree.eptp));
}
static void positive46(void){
 struct maintenance_fixture *m=make46();struct fixture *f=&m->f;
 uint64_t life[2],hpa[2];
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->owners[c];select_root(o,2);struct nested_ept_result r;
  assert(fault46(o,0x7000,1,&r)==TR_ATOMIC_FILLED&&r.leaf_type==6&&r.hpa==DATA+0x4000);
  select_root(o,c);life[c]=active(o)->directory.identity.allocator_lifetime;hpa[c]=o->registry.active->tree.eptp;
 }
 for(unsigned round=0;round<3;round++){
  /* Both read before D is set, then each needs its own first write. */
  for(unsigned c=0;c<2;c++)read46(&f->owners[c]);
  assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x100);
  promote46(&f->owners[0]);assert(leaf46(&f->owners[1],0x7000)==(DATA|0x41));promote46(&f->owners[1]);
  assert(__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)==0x200343);
  if(!round)for(unsigned c=0;c<2;c++)pin46(m,c);
  if(round==2)break;
  struct nac_trace old=f->owners[0].traces[round+1].trace;(void)old;
  mreset(m);
  assert(__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)==0x200043);
  assert(f->lease.type==TRL_UC&&f->lease.type_epoch==17&&f->catalog.type_epoch==17&&m->maintenance.type_epoch==17);
  for(unsigned c=0;c<2;c++){f->cpu=c;assert(tam_prepare(&m->maintenance,c,m->ticket));}
  assert(tam_finish(&m->maintenance,m->ticket)&&f->gate.stamp.epoch==6+round);
  for(unsigned c=0;c<2;c++){
   struct owner *o=&f->owners[c];assert(active(o)->directory.identity.allocator_lifetime==life[c]&&o->registry.active->tree.eptp==hpa[c]);
   for(unsigned n=0;n<TRACES;n++)if(o->traces[n].used)assert(!o->traces[n].registered&&!o->traces[n].usable);
  }
 }
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->owners[c];f->cpu=c;assert(nested_ept_contexts_unpin(&o->registry,o->registry.active));
  assert(nested_ept_contexts_destroy(&o->registry)&&!o->pool.live&&o->allocations==o->frees);
  for(unsigned n=0;n<o->reserved;n++)assert(o->bindings[n].released&&!o->bindings[n].pin.live&&tr_local_completed_matches(&o->provider,&o->slots[n]));
 }
 assert(!f->catalog.references&&!f->directory.records&&f->lease.type_epoch==17&&f->lease.type==TRL_UC);
 assert(trl_close(&f->lease));struct trl_freeze freeze;assert(trl_seal(&f->lease,&freeze));
 struct trl_final_evidence evidence46={0};assert(!trl_backing_release_ready(&f->lease,freeze,&evidence46));
 free(m);puts("PASS N46A fixed UC two-owner two-reset retained teardown");
}
static void after_peer46(void){
 struct maintenance_fixture *m=make46();for(unsigned c=0;c<2;c++)select_root(&m->f.owners[c],c);
 read46(&m->f.owners[0]);promote46(&m->f.owners[0]);
 read46(&m->f.owners[1]);assert(__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)==0x200343);
 promote46(&m->f.owners[1]);free(m);puts("PASS N46A peer dirty cannot grant private W");
}
static void authority_fail46(void){
 for(unsigned bad=1;bad<=6;bad++){
  struct maintenance_fixture *m=make46();struct owner *o=&m->f.owners[0];select_root(o,0);auth_bad46=bad;
  struct nested_ept_result r;assert(fault46(o,0x7000,2,&r)==TR_ATOMIC_FATAL&&r.status==NESTED_EPT_L0_BACKING_DENIED);
  assert(tables[0][0]==(gpas[2]|7)&&tables[4][7]==0x200043&&o->pool.live&&m->f.catalog.references==1);
  free(m);printf("PASS N46A authority failure %u\n",bad);
 }
}
static void format_fail46(void){
 for(unsigned bad=0;bad<10;bad++){
  struct maintenance_fixture *m=make46();struct owner *o=&m->f.owners[0];select_root(o,0);
  uint64_t raw=0x200043;
  switch(bad){
  case 0:raw|=8;break;case 1:raw|=16;break;case 2:raw|=24;break;
  case 3:raw|=32;break;case 4:raw|=40;break;case 5:raw|=56;break;
  case 6:raw&=~UINT64_C(0x40);break;case 7:raw|=4;break;
  case 8:raw|=0x80;break;case 9:tables[3][0]=0x2000c3;break;
  }
  if(bad!=9)tables[4][7]=raw;
  struct nested_ept_result r;assert(fault46(o,0x7000,1,&r)==TR_ATOMIC_FATAL&&r.status==NESTED_EPT_UNSUPPORTED);
  assert(!(tables[0][0]&0x100));free(m);printf("PASS N46A source format failure %u\n",bad);
 }
}
static void trace_fail46(void){
 for(unsigned bad=0;bad<12;bad++){
  struct maintenance_fixture *m=make46();struct owner *o=&m->f.owners[0];select_root(o,0);read46(o);
  struct nac_trace *t=&o->traces[0].trace;
  switch(bad){
  case 0:t->type_policy.epoch++;break;case 1:t->type_policy.uc_hpa+=4096;break;
  case 2:t->type_policy.uc_gpa+=4096;break;case 3:t->translation.leaf_type=6;break;
  case 4:t->path.stamp.epoch++;break;case 5:t->cache.generation++;break;
  case 6:t->cache.lifetime++;break;case 7:o->bindings[0].pin_nonce++;break;
  case 8:o->slots[0].typed.token.type_epoch++;break;case 9:m->f.catalog.type_epoch++;break;
  case 10:t->path.entry[3].raw^=0x30;break;case 11:t->path.entry[3].pointer=(void *)1;break;
  }
  unsigned old=o->flushes;struct nested_ept_result r;
  enum tr_atomic_status status=fault46(o,0x7000,2,&r);assert(status==TR_ATOMIC_FATAL||status==TR_ATOMIC_REJECTED);
  assert(tables[4][7]==0x200143&&leaf46(o,0x7000)==(DATA|0x41)&&o->flushes==old);
  free(m);printf("PASS N46A trace identity failure %u\n",bad);
 }
}
static void maintenance_fail46(void){
 for(unsigned bad=0;bad<15;bad++){
  struct maintenance_fixture *m=make46();for(unsigned c=0;c<2;c++){select_root(&m->f.owners[c],c);read46(&m->f.owners[c]);promote46(&m->f.owners[c]);pin46(m,c);}
  if(bad<9){m->corrupt_ack=bad+1;assert(tam_close(&m->maintenance,&m->ticket)&&tam_drain(&m->maintenance,m->ticket)==TAM_DRAIN_READY);
   for(unsigned c=0;c<2;c++){m->f.cpu=c;assert(tam_revoke(&m->maintenance,c,m->ticket));}
   assert(tam_clear(&m->maintenance,m->ticket));m->f.cpu=0;assert(!tam_reset(&m->maintenance,0,m->ticket));
  }else{
   assert(tam_close(&m->maintenance,&m->ticket)&&tam_drain(&m->maintenance,m->ticket)==TAM_DRAIN_READY);
   for(unsigned c=0;c<2;c++){m->f.cpu=c;assert(tam_revoke(&m->maintenance,c,m->ticket));}
   switch(bad){
   case 9:tables[8][7]=0x208043;break; /* UC source on WB non-target, not current root */
   case 10:tables[4][7]|=0x30;break; /* WB source on UC target */
   case 11:tables[4][7]|=4;break;
   case 12:m->fail_cas=1;break;case 13:m->break_guard=true;break;case 14:m->f.catalog.type_epoch++;break;
   }
   assert(!tam_clear(&m->maintenance,m->ticket));
  }
  assert(m->maintenance.state==(bad==14?TAM_DRAINED:TAM_POISONED)&&m->f.catalog.references==2&&m->f.owners[0].pool.live&&m->f.owners[1].pool.live);
  free(m);printf("PASS N46A maintenance failure %u\n",bad);
 }
}
static void constructor46(void){
 for(unsigned bad=0;bad<15;bad++){
  struct nested_root_catalog c={0};struct nrc_span spans[4];struct nrc_root roots[1];struct nrc_identity id={1,2,3,4};
  assert(nrc_init_atomic_uc(&c,id,48,spans,4,roots,1,authority46,0x200000,DATA,17));
  struct nrc_span s={.identity=id,.object=1,.lifetime=1,.gpa=0x200000,.hpa=DATA,.bytes=4096,.permissions=3,.allowed_types=NRC_TYPE_UC,.role=NRC_DATA,.ignore_pat=true};
  switch(bad){
  case 0:s.allowed_types=NRC_TYPE_WB;break;case 1:s.allowed_types=NRC_TYPE_UC|NRC_TYPE_WB;break;
  case 2:s.allowed_types=1u<<1;break;case 3:s.allowed_types=1u<<4;break;case 4:s.allowed_types=1u<<5;break;
  case 5:s.permissions|=4;break;case 6:s.bytes=8192;break;case 7:s.ignore_pat=false;break;
  case 8:s.gpa+=4096;break;case 9:s.hpa+=4096;break;case 10:s.role=NRC_TABLE;s.permissions=1;s.table_view=tables[0];break;
  case 11:s.gpa-=4096;s.bytes=8192;s.allowed_types=NRC_TYPE_WB;break;
  case 12:s.hpa-=4096;s.bytes=8192;s.allowed_types=NRC_TYPE_WB;break;
  case 13:s.table_view=tables[0];break;case 14:s.role=3;break;
  }
  assert(!nrc_add_atomic_span(&c,s,s.role==NRC_TABLE?tables[0]:NULL)&&!c.span_count);
  printf("PASS N46A catalog registration failure %u\n",bad);
 }
 struct nested_root_catalog c={.references=123},before=c;struct nrc_span spans[1];struct nrc_root roots[1];
 assert(!nrc_init_atomic_uc(&c,(struct nrc_identity){1,2,3,4},48,spans,1,roots,1,authority46,0x200000,DATA,0)&&!memcmp(&c,&before,sizeof c));
 puts("PASS N46A constructor unchanged on rejection");
}
int main(void){setvbuf(stdout,NULL,_IONBF,0);positive46();after_peer46();authority_fail46();format_fail46();trace_fail46();maintenance_fail46();constructor46();}
