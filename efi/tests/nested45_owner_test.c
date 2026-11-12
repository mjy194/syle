/* Existing production physical-owner fixture, constructor policy only changed.
 * Allocation/VMCS/INVEPT remain mocks; metadata and ownership callbacks unchanged. */
#include "n45_owner_fixture.inc"
static void check_leaf(struct fixture *f,uint64_t gpa,bool writable){
 uint64_t leaf=0;assert(nested_owner_atomic_leaf(&f->owner,gpa,&leaf));
 assert((leaf&0x78)==0x70&&!!(leaf&2)==writable);
}
int main(void){
 struct shared *g=make();pthread_t threads[2];
 for(unsigned c=0;c<2;c++){g->f[c].owner.target_gpa=0x4000f000;assert(!pthread_create(&threads[c],0,bootstrap_thread,&g->f[c]));}
 for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],0));
 for(unsigned c=0;c<2;c++){
  actual_cpu=c;struct fixture *f=&g->f[c];
  f->sparse.hardware_item=nested_ept_contexts_pin_active(&f->sparse.contexts);
  f->composed=f->sparse.hardware_item->tree.eptp;
  assert(nested_owner_atomic_entry(&f->owner,f->vcpu.vmcs02,f->composed,true));
 }
 actual_cpu=0;assert(nested_owner_atomic_fault(&g->f[0].owner,0x4000f000,0x181,0));check_leaf(&g->f[0],0x4000f000,false);
 assert(nested_owner_atomic_fault(&g->f[0].owner,0x4000f008,0x18a,0));check_leaf(&g->f[0],0x4000f000,true);
 assert((ram[16][1]&0x300)==0x300);
 for(unsigned c=0;c<2;c++){
  actual_cpu=c;struct fixture *f=&g->f[c];
  if(c){assert(nested_owner_atomic_fault(&f->owner,0x4000f000,0x181,0));check_leaf(f,0x4000f000,false);}
  assert(nested_owner_atomic_fault(&f->owner,0x40200000,0x181,0));check_leaf(f,0x40200000,false);
  assert(nested_owner_atomic_fault(&f->owner,0x40200008,0x18a,0));check_leaf(f,0x40200000,true);
  if(c){check_leaf(f,0x4000f000,false);assert(nested_owner_atomic_fault(&f->owner,0x4000f008,0x18a,0));}
  assert(nested_owner_atomic_fault(&f->owner,0x7ffff000,0x181,0));check_leaf(f,0x7ffff000,false);
  assert(nested_owner_atomic_fault(&f->owner,0x7ffff008,0x18a,0));check_leaf(f,0x7ffff000,true);
  assert(f->owner.target_promotions[0]==1&&f->owner.promotions==3);
  unsigned found=0;
  for(unsigned n=0;n<NESTED_ATOMIC_TRACES;n++)if(f->owner.traces[n].usable){
   const struct nac_trace *t=&f->owner.traces[n].trace;
   if(t->gpa==0x4000f000||t->gpa==0x40200000||t->gpa==0x7ffff000){
    assert(t->path.count==2&&t->path.entry[1].depth==1&&t->path.entry[1].leaf_shift==30);
    assert(t->path.entry[1].entry_gpa==0x11008&&t->path.entry[1].pointer==&ram[16][1]);found++;
   }
  }
  assert(found==3);
 }
 assert(nested_atomic_memory_ready(&m)&&m.catalog.references==2);
 puts("PASS N45 production physical-owner faults: two owners/three subpages, peer-D read-only, separate write promotions, retained 1GiB traces; VMCS primitives mocked");return 0;
}
