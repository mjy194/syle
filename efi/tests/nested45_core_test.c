/* Actual typed owner/catalog/walker/TAM. Physical primitives are mocks.
 * All source structure is immutable after publication; deliberate mutations
 * below are rejection tests, not a supported live-update protocol. */
#include "n45_fixture_base.inc"
#define G45 UINT64_C(0x40000000)
static const uint64_t offsets45[]={0,0x1000,0x1ff000,0x200000,0x12345000,0x3fffe000,0x3ffff000};
static const uint64_t hpas45[]={DATA,DATA+0x9000,DATA+0x3000,DATA+0x17000,DATA+0x5000,DATA+0x1c000,DATA+0x1000};
static uint64_t get45(const uint64_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static struct maintenance_fixture *make45(void){
 struct maintenance_fixture *m=calloc(1,sizeof *m);assert(m);struct fixture *f=&m->f;
 memset(tables,0,sizeof tables);f->id=(struct nrc_identity){1,2,3,4};f->stamp=(struct ad_entry_stamp){1,2,3,5,4};
 assert(nrc_init_atomic_wb_1g(&f->catalog,f->id,48,f->spans,18,f->roots,3,mauthority));
 for(unsigned i=0;i<9;i++){
  struct nrc_span s={.identity=f->id,.object=i+1,.lifetime=50+i,.gpa=gpas[i],.hpa=0x100000+i*4096,
   .bytes=4096,.table_view=tables[i],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,tables[i]));
 }
 for(unsigned i=0;i<8;i++){
  struct nrc_span s={.identity=f->id,.object=20+i,.lifetime=80+i,.gpa=i<7?G45+offsets45[i]:2*G45,
   .hpa=i<7?hpas45[i]:DATA+0x22000,.bytes=4096,.permissions=i==5?1:3,
   .allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,NULL));
 }
 const unsigned indices[]={0,1,5};
 for(unsigned i=0;i<3;i++){
  uint64_t root=gpas[indices[i]];
  assert(nrc_add_root(&f->catalog,(struct nrc_root){f->id,30+i,i,10+i,90+i,root|0x5e,root}));
 }
 tables[0][0]=tables[1][0]=gpas[2]|7;tables[2][0]=gpas[3]|7;tables[3][0]=gpas[4]|7;
 tables[4][7]=G45|0x73;tables[3][1]=2*G45|0xf3;tables[2][1]=G45|0xf3;tables[2][2]=2*G45|0xf3;
 tables[5][0]=gpas[6]|7;tables[6][0]=gpas[7]|7;tables[7][0]=gpas[8]|7;
 tables[8][7]=G45|0x73;tables[6][1]=G45|0xf1;tables[6][2]=UINT64_C(0xfffffffffffffff8);
 assert(nrc_publish(&f->catalog)&&ad_updater_gate_init(&f->gate,f->stamp));
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
static enum tr_atomic_status fault45(struct owner *o,uint64_t gpa,unsigned access,struct nested_ept_result *out){
 o->f->cpu=o->cpu;return tr_atomic_fault(&o->atomic,o->registry.active,gpa,access,out);
}
static uint64_t leaf45(struct owner *o,uint64_t gpa){
 uint64_t hpa=o->registry.active->tree.eptp&~UINT64_C(4095),raw=0;
 for(unsigned depth=0;depth<4;depth++){
  uint64_t *page=NULL;for(unsigned n=0;n<PAGES;n++)if(o->pages[n].hpa==hpa)page=o->pages[n].view;
  assert(page);raw=page[(gpa>>(39-depth*9))&511];
  if(depth<3){assert((raw&4095)==7);hpa=raw&~UINT64_C(4095);}
 }
 assert(!(raw&0x80));return raw;
}
static void read45(struct owner *o,uint64_t gpa,uint64_t l1,uint64_t hpa,unsigned depth){
 struct nested_ept_result out;assert(fault45(o,gpa,1,&out)==TR_ATOMIC_FILLED);
 assert(out.status==NESTED_EPT_OK&&out.hpa==hpa&&out.l1_gpa==l1&&leaf45(o,gpa)==(hpa|0x71));
 bool found=false;
 for(unsigned i=0;i<TRACES;i++)if(o->traces[i].used&&o->traces[i].trace.gpa==gpa){
  struct nested_ept_atomic_trace *t=&o->traces[i].trace.path;
  assert(t->count==depth+1&&t->entry[depth].depth==depth&&t->entry[depth].leaf&&
   t->entry[depth].leaf_shift==(depth==1?30:depth==2?21:12));found=true;
 }
 /* Read-only L0 backing does not need a retained first-write trace. */
 assert(found||gpa==G45+offsets45[5]);
}
static void promote45(struct owner *o,uint64_t gpa){
 struct nested_ept_result out;unsigned before=o->flushes;
 assert(fault45(o,gpa,2,&out)==TR_ATOMIC_PROMOTED&&o->flushes==before+1);
 assert((leaf45(o,gpa)&0xfff)==0x73);
}
static void pin45(struct maintenance_fixture *m,unsigned c){
 struct owner *o=&m->f.owners[c];m->f.cpu=c;
 struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);assert(item);
 m->references[c]=item->tree.eptp;assert(tr_atomic_entry(&o->atomic,item,item->tree.eptp));
}
static void normal45(void){
 struct maintenance_fixture *m=make45();struct fixture *f=&m->f;
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->owners[c];select_root(o,2);struct nested_ept_result out;
  read45(o,0x7000,G45,DATA,3);promote45(o,0x7000);
  assert(fault45(o,G45+offsets45[6],2,&out)==TR_ATOMIC_DENIED&&out.status==NESTED_EPT_L1_DENIED);
  assert(fault45(o,2*G45,1,&out)==TR_ATOMIC_DENIED&&!out.leaf_resolved&&out.effective12==0);
  select_root(o,c);
 }
 for(unsigned round=0;round<3;round++){
  for(unsigned c=0;c<2;c++){
   struct owner *o=&f->owners[c];
   read45(o,0x7000,G45,DATA,3);promote45(o,0x7000);
   read45(o,0x200000,2*G45,DATA+0x22000,2);promote45(o,0x200000);
   /* Some mappings predate D and others are first read after another
    * far subpage/owner sets D. Every private 4K leaf must remain RO. */
   for(unsigned n=0;n<7;n++){
    read45(o,G45+offsets45[n],G45+offsets45[n],hpas45[n],1);
    if(n!=5)promote45(o,G45+offsets45[n]);
   }
   read45(o,2*G45,2*G45,DATA+0x22000,1);promote45(o,2*G45);
   assert(get45(&tables[2][1])==(G45|0x3f3));
   if(!round)pin45(m,c);
  }
  if(round==2)break;
  mreset(m);assert(m->maintenance.page_count==9&&m->maintenance.root_count[0]==2&&m->maintenance.root_count[1]==2);
  assert(get45(&tables[2][1])==(G45|0xf3)&&get45(&tables[2][2])==(2*G45|0xf3));
  assert(get45(&tables[3][1])==(2*G45|0xf3)&&get45(&tables[4][7])==(G45|0x73));
  assert(get45(&tables[6][1])==(G45|0xf1)&&get45(&tables[6][2])==UINT64_C(0xfffffffffffffff8));
  assert(m->maintenance.cleared_entries==13&&m->acks==4*(round+1));
  for(unsigned c=0;c<2;c++){f->cpu=c;assert(tam_prepare(&m->maintenance,c,m->ticket));}
  assert(tam_finish(&m->maintenance,m->ticket)&&f->gate.stamp.epoch==6+round);
 }
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->owners[c];f->cpu=c;assert(nested_ept_contexts_unpin(&o->registry,o->registry.active));
  assert(nested_ept_contexts_destroy(&o->registry)&&!o->pool.live&&o->allocations==o->frees);
 }
 assert(!f->catalog.references&&!f->directory.records);free(m);
 puts("PASS N45 private two-owner noncontiguous far subpages, 4K/2M/1G two-reset retrack and retained teardown");
}
static void fail45(void){
 for(unsigned mode=0;mode<12;mode++){
  struct maintenance_fixture *m=make45();struct owner *o=&m->f.owners[0];select_root(o,0);struct nested_ept_result out;
  if(mode<2){
   uint64_t gpa=G45+(mode?offsets45[5]:0x2000);
   assert(fault45(o,gpa,mode?2:1,&out)==TR_ATOMIC_FATAL&&out.status==NESTED_EPT_L0_BACKING_DENIED);
  }else{
   read45(o,G45+offsets45[6],G45+offsets45[6],hpas45[6],1);
   struct nac_trace *t=&o->traces[0].trace;
   switch(mode){
   case 2:__atomic_fetch_and(&tables[2][1],~UINT64_C(2),__ATOMIC_ACQ_REL);break;
   case 3:__atomic_store_n(&tables[2][1],UINT64_C(0xdead000),__ATOMIC_RELEASE);break;
   case 4:t->path.entry[1].depth=2;break;
   case 5:t->path.entry[1].leaf_shift=21;break;
   case 6:t->path.entry[1].leaf=false;break;
   case 7:t->path.count=3;break;
   case 8:t->path.count=1;break;
   case 9:t->path.entry[1].pointer=(void *)1;break;
   case 10:t->translation.l1_gpa-=0x200000;break;
   case 11:t->path.entry[1].entry_gpa+=8;break;
   }
   assert(fault45(o,G45+offsets45[6],2,&out)==TR_ATOMIC_FATAL);
  }
  assert(o->atomic.poisoned&&o->bindings[0].pin.live&&o->pool.live&&out.status!=NESTED_EPT_MISCONFIG);
  assert(!(get45(&tables[2][1])&0x200));free(m);printf("PASS N45 fault case %u\n",mode);
 }
}
static void maintenance_fail45(void){
 for(unsigned mode=0;mode<10;mode++){
  struct maintenance_fixture *m=make45();
  for(unsigned c=0;c<2;c++){
   struct owner *o=&m->f.owners[c];select_root(o,c);read45(o,G45,G45,DATA,1);promote45(o,G45);pin45(m,c);
  }
  assert(tam_close(&m->maintenance,&m->ticket)&&tam_drain(&m->maintenance,m->ticket)==TAM_DRAIN_READY);
  for(unsigned c=0;c<2;c++){m->f.cpu=c;assert(tam_revoke(&m->maintenance,c,m->ticket));}
  switch(mode){
  case 0:__atomic_fetch_or(&tables[2][1],0x1000,__ATOMIC_ACQ_REL);break;
  case 1:__atomic_fetch_or(&tables[2][1],0x200000,__ATOMIC_ACQ_REL);break;
  case 2:__atomic_fetch_or(&tables[2][1],UINT64_C(1)<<29,__ATOMIC_ACQ_REL);break;
  case 3:__atomic_fetch_or(&tables[2][1],UINT64_C(1)<<48,__ATOMIC_ACQ_REL);break;
  case 4:__atomic_fetch_or(&tables[0][0],0x80,__ATOMIC_ACQ_REL);break;
  case 5:__atomic_fetch_or(&tables[4][7],0x80,__ATOMIC_ACQ_REL);break;
  case 6:__atomic_fetch_and(&tables[2][1],~UINT64_C(0x40),__ATOMIC_ACQ_REL);break;
  case 7:__atomic_store_n(&tables[6][3],gpas[4]|7,__ATOMIC_RELEASE);break; /* wrong-depth reuse */
  case 8:m->fail_cas=1;break;
  case 9:m->fail_cas=2;break;
  }
  assert(!tam_clear(&m->maintenance,m->ticket)&&m->maintenance.state==TAM_POISONED);
  assert(m->f.catalog.references==2&&m->f.owners[0].pool.live&&m->f.owners[1].pool.live);
  assert(m->maintenance.changed==(mode==9?1u:0u));free(m);printf("PASS N45 maintenance case %u\n",mode);
 }
}
int main(void){normal45();fail45();maintenance_fail45();}
