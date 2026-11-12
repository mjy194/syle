/* Added to a fresh transformed N43 helper prefix by nested45_memory.py. */
int main(void){
 struct multifixture *x=create_multi();
 for(unsigned round=1;round<=2;round++){
  x->generation=round;execute_round(x,true);all_roots(x,round);
  assert(x->g->runtime.maintenance.page_count==11);
  for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){
   struct fixture *f=&x->g->f[c];struct tam_root *p=&x->g->runtime.maintenance.roots[c][n];select_item(f,p->item);
   const uint64_t targets[]={0x40200000,0x7ffff000};
   for(unsigned k=0;k<2;k++){
    uint64_t leaf=0;assert(nested_owner_atomic_fault(&f->owner,targets[k],0x181,0));
    assert(nested_owner_atomic_leaf(&f->owner,targets[k],&leaf)&&!(leaf&2));
    assert(nested_owner_atomic_fault(&f->owner,targets[k]+8,0x18a,0));
    assert(nested_owner_atomic_leaf(&f->owner,targets[k],&leaf)&&(leaf&2));
   }
  }
 }
 assert(x->g->outer.epoch==3&&nested_atomic_memory_ready(&m));
 puts("PASS N45 actual sparse fixture two TAM reset rounds, 11-table graph, four retained roots and far-offset independent first-write traps");
 free(x->g);free(x);return 0;
}
