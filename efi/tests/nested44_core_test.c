/* Production typed owner/catalog/walker/maintenance test. Physical primitives
 * are mocks. Source words are built before publish, then immutable except A/D
 * or explicit fail-closed corruption injection. No source-update claim. */
#include "n44_fixture_base.inc"
static uint64_t get44(const uint64_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static struct maintenance_fixture *make44(void){
 struct maintenance_fixture *m=calloc(1,sizeof *m);assert(m);struct fixture *f=&m->f;
 memset(tables,0,sizeof tables);
 f->id=(struct nrc_identity){1,2,3,4};f->stamp=(struct ad_entry_stamp){1,2,3,5,4};
 assert(nrc_init_atomic_wb_2m(&f->catalog,f->id,48,f->spans,11,f->roots,3,mauthority));
 const unsigned indices[]={0,1,2,3,5,6,7};
 for(unsigned j=0;j<7;j++){
  unsigned i=indices[j];struct nrc_span s={.identity=f->id,.object=i+1,.lifetime=50+i,
   .gpa=gpas[i],.hpa=0x100000+i*4096,.bytes=4096,.table_view=tables[i],
   .permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,tables[i]));
 }
 const uint64_t data_gpa[]={0x200000,0x208000,0x3ff000,0x400000},hpa[]={DATA,DATA+0x4000,DATA+0x9000,DATA+0x1000};
 for(unsigned i=0;i<4;i++){
  struct nrc_span s={.identity=f->id,.object=20+i,.lifetime=80+i,.gpa=data_gpa[i],.hpa=hpa[i],
   .bytes=4096,.permissions=i==2?1:3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,NULL));
 }
 const unsigned roots[]={0,1,5};
 for(unsigned i=0;i<3;i++){
  uint64_t root=gpas[roots[i]];
  assert(nrc_add_root(&f->catalog,(struct nrc_root){f->id,30+i,i,10+i,90+i,root|0x5e,root}));
 }
 tables[0][0]=tables[1][0]=gpas[2]|7;tables[2][0]=gpas[3]|7;
 tables[3][0]=0x2000f3;tables[3][1]=0x4000f3;
 tables[5][0]=gpas[6]|7;tables[6][0]=gpas[7]|7;tables[7][0]=0x2000f1;
 tables[7][1]=UINT64_C(0xfffffffffffffff8); /* Nonpresent payload is ignored. */
 assert(nrc_publish(&f->catalog));assert(ad_updater_gate_init(&f->gate,f->stamp));
 assert(td_init(&f->directory,f->id.domain,f->id.incarnation,3,1));
 assert(trl_init_atomic_wb(&f->lease,&f->directory,(struct trl_backing){DATA,99,true},2*SLOTS));
 for(unsigned cpu=0;cpu<2;cpu++){
  struct owner *o=&f->owners[cpu];o->f=f;o->cpu=cpu;o->stopped=true;
  for(unsigned i=0;i<PAGES;i++)o->pages[i]=(struct nested_sparse_page){0x900000+cpu*0x100000+i*4096,memory[cpu][i]};
  assert(nested_owner_pool_init(&o->pool,cpu,100+cpu,o->pages,o->pool_slots,PAGES,48));
  const struct nested_sparse_ops sparse={alloc,release,flush};
  assert(nested_ept_contexts_init_config(&o->registry,&sparse,o,48,2,true,true,NULL,NULL));
  const struct tr_registry_root_provider provider={reserve,evidence};
  const struct tr_atomic_config config={.catalog=&f->catalog,.catalog_identity=f->id,.authority_context=o,
   .gate=&f->gate,.stamp=f->stamp,.slots=o->slots,.bindings=o->bindings,.slot_count=SLOTS,
   .traces=o->traces,.trace_count=TRACES,.lock=lock,.unlock=unlock,.current_cpu=current_cpu,.consumer_stopped=stopped,.context=o};
  assert(tr_atomic_init(&o->atomic,&o->bridge,&o->registry,&f->lease,cpu,&provider,o,&config));
  o->provider=(struct tr_local_provider){.bridge=&o->bridge,.slots=o->slots,.completed=o->completed,
   .slot_count=SLOTS,.owner=cpu,.serial=&f->serial,.lock=provider_lock,.unlock=unlock,
   .current_cpu=current_cpu,.healthy=provider_healthy,.invalidate=invalidate,.context=o};
 }
 const struct tam_hooks hooks={mclose,mstopped,mremove,mflush,mrestore,mopen,mload,mcas};
 assert(tam_init(&m->maintenance,&f->owners[0].atomic,&f->owners[1].atomic,&hooks,m));return m;
}
static enum tr_atomic_status fault44(struct owner *o,uint64_t gpa,unsigned access,struct nested_ept_result *out){
 o->f->cpu=o->cpu;return tr_atomic_fault(&o->atomic,o->registry.active,gpa,access,out);
}
static uint64_t leaf44(struct owner *o,uint64_t gpa){
 uint64_t hpa=o->registry.active->tree.eptp&~UINT64_C(4095),raw=0;
 for(unsigned depth=0;depth<4;depth++){
  uint64_t *page=NULL;for(unsigned n=0;n<PAGES;n++)if(o->pages[n].hpa==hpa)page=o->pages[n].view;
  assert(page);raw=page[(gpa>>(39-depth*9))&511];
  if(depth<3){assert((raw&4095)==7);hpa=raw&~UINT64_C(4095);}
 }
 assert(!(raw&0x80));return raw;
}
static void read44(struct owner *o,uint64_t gpa,uint64_t hpa){
 struct nested_ept_result out;assert(fault44(o,gpa,1,&out)==TR_ATOMIC_FILLED);
 assert(out.status==NESTED_EPT_OK&&out.hpa==hpa&&out.l1_gpa==0x200000+gpa);
 assert(leaf44(o,gpa)==(hpa|0x71));
}
static void promote44(struct owner *o,uint64_t gpa){
 struct nested_ept_result out;unsigned before=o->flushes;
 assert(fault44(o,gpa,2,&out)==TR_ATOMIC_PROMOTED&&o->flushes==before+1);
 assert((leaf44(o,gpa)&0xfff)==0x73);
}
static void normal44(void){
 struct maintenance_fixture *m=make44();struct fixture *f=&m->f;
 struct owner *a=&f->owners[0],*b=&f->owners[1];select_root(a,0);select_root(b,1);
 read44(a,0,DATA);read44(a,0x8000,DATA+0x4000);read44(b,0,DATA);read44(b,0x8000,DATA+0x4000);
 assert(get44(&tables[3][0])==0x2001f3);promote44(a,0);assert(get44(&tables[3][0])==0x2003f3);
 /* Existing and new peer mappings must trap independently despite shared D. */
 promote44(b,0x8000);promote44(a,0x8000);promote44(b,0);
 read44(a,0x1ff000,DATA+0x9000);read44(a,0x200000,DATA+0x1000);promote44(a,0x200000);
 assert(get44(&tables[3][1])==0x4003f3);
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->owners[c];select_root(o,2);struct nested_ept_result out;
  assert(fault44(o,0,2,&out)==TR_ATOMIC_DENIED&&out.status==NESTED_EPT_L1_DENIED);
  assert(fault44(o,0x200000,1,&out)==TR_ATOMIC_DENIED&&out.status==NESTED_EPT_L1_DENIED);
  assert(!out.leaf_resolved&&out.effective12==0);
  select_root(o,c);struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);assert(item);
  m->references[c]=item->tree.eptp;assert(tr_atomic_entry(&o->atomic,item,item->tree.eptp));
 }
 assert(get44(&tables[7][0])==0x2001f1&&get44(&tables[7][1])==UINT64_C(0xfffffffffffffff8));
 mreset(m);assert(m->maintenance.page_count==7&&m->maintenance.root_count[0]==2&&m->maintenance.root_count[1]==2);
 assert(get44(&tables[3][0])==0x2000f3&&get44(&tables[3][1])==0x4000f3&&get44(&tables[7][0])==0x2000f1);
 assert(get44(&tables[7][1])==UINT64_C(0xfffffffffffffff8));
 assert(m->maintenance.changed==8&&m->maintenance.cleared_entries==8&&m->acks==4);
 for(unsigned c=0;c<2;c++){f->cpu=c;assert(tam_prepare(&m->maintenance,c,m->ticket));}
 assert(tam_finish(&m->maintenance,m->ticket));assert(f->gate.stamp.epoch==6);
 read44(a,0,DATA);promote44(a,0);read44(b,0x8000,DATA+0x4000);promote44(b,0x8000);
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->owners[c];f->cpu=c;assert(nested_ept_contexts_unpin(&o->registry,o->registry.active));
  assert(nested_ept_contexts_destroy(&o->registry)&&!o->pool.live&&o->allocations==o->frees);
 }
 assert(!f->catalog.references&&!f->directory.records);free(m);
 puts("PASS N44 private 4K traps, noncontiguous offsets, immutable denial/unmap, huge-leaf retrack and retained teardown");
}
static void fail44(void){
 for(unsigned mode=0;mode<6;mode++){
  struct maintenance_fixture *m=make44();struct owner *a=&m->f.owners[0];select_root(a,0);
  struct nested_ept_result out;
  if(mode==0){assert(fault44(a,0x1000,1,&out)==TR_ATOMIC_FATAL);assert(out.status==NESTED_EPT_L0_BACKING_DENIED);}
  if(mode==1){assert(fault44(a,0x1ff000,2,&out)==TR_ATOMIC_FATAL);assert(out.status==NESTED_EPT_L0_BACKING_DENIED);}
  if(mode>=2){
   read44(a,0,DATA);
   if(mode==2)__atomic_fetch_and(&tables[3][0],~UINT64_C(2),__ATOMIC_ACQ_REL);
   if(mode==3)__atomic_store_n(&tables[3][0],UINT64_C(0xdead000),__ATOMIC_RELEASE);
   if(mode==4)a->traces[0].trace.path.entry[2].depth=3;
   if(mode==5)a->traces[0].trace.path.entry[2].leaf_shift=12;
   assert(fault44(a,0,2,&out)==TR_ATOMIC_FATAL);
  }
  assert(a->atomic.poisoned&&a->bindings[0].pin.live&&a->pool.live&&out.status!=NESTED_EPT_MISCONFIG);
  assert(!(get44(&tables[3][0])&0x200));free(m);
  printf("PASS N44 fault case %u\n",mode);
 }
 puts("PASS N44 backing holes/permissions and live structural or durable trace corruption fail closed");
}
static void maintenance_fail44(void){
 for(unsigned mode=0;mode<5;mode++){
  struct maintenance_fixture *m=make44();
  for(unsigned c=0;c<2;c++){
   struct owner *o=&m->f.owners[c];select_root(o,c);read44(o,0,DATA);promote44(o,0);
   struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);assert(item);
   m->references[c]=item->tree.eptp;
  }
  assert(tam_close(&m->maintenance,&m->ticket));
  assert(tam_drain(&m->maintenance,m->ticket)==TAM_DRAIN_READY);
  for(unsigned c=0;c<2;c++){m->f.cpu=c;assert(tam_revoke(&m->maintenance,c,m->ticket));}
  if(mode==0)__atomic_fetch_or(&tables[3][0],0x1000,__ATOMIC_ACQ_REL);
  if(mode==1)__atomic_fetch_or(&tables[2][0],0x80,__ATOMIC_ACQ_REL);
  if(mode==2)__atomic_fetch_or(&tables[7][0],UINT64_C(1)<<63,__ATOMIC_ACQ_REL);
  if(mode==3)m->fail_cas=1;
  if(mode==4)m->fail_cas=2;
  assert(!tam_clear(&m->maintenance,m->ticket)&&m->maintenance.state==TAM_POISONED);
  assert(m->f.catalog.references==2&&m->f.owners[0].pool.live&&m->f.owners[1].pool.live);
  assert(m->maintenance.changed==(mode==4?1u:0u));free(m);
  printf("PASS N44 maintenance case %u\n",mode);
 }
 puts("PASS N44 huge maintenance format/CAS failure retains all source and cache backing");
}
int main(void){normal44();fail44();maintenance_fail44();}
