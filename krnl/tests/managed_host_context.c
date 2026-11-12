#include "../managed_host_context.h"
#include <assert.h>
#include <stdio.h>
struct mock{uint64_t root;struct mhc_selectors selectors;unsigned roots,sets;};
static uint64_t root(void*p){return ((struct mock*)p)->root;}
static void setroot(void*p,uint64_t r){struct mock*m=p;m->root=r;m->roots++;}
static struct mhc_selectors get(void*p){return ((struct mock*)p)->selectors;}
static void set(void*p,struct mhc_selectors s){struct mock*m=p;m->selectors=s;m->sets++;}
int main(void){
 struct mock m={.root=0x2000,.selectors={0x33,0x33,0}};struct mhc_ops o={&m,root,setroot,get,set};struct mhc_saved s={0};
 assert(m.root!=0x1000); /* unchanged strict WB proof rejects caller root */
 assert(mhc_enter(&s,&o,0x1000));assert(m.root==0x1000&&m.selectors.ss==0x10&&m.roots==1);
 assert(!mhc_enter(&s,&o,0x1000));assert(mhc_leave(&s,&o));assert(m.root==0x2000&&m.selectors.ds==0x33&&m.selectors.es==0x33&&!m.selectors.ss&&m.roots==2);
 assert(!mhc_leave(&s,&o));
 m=(struct mock){.root=0x1000,.selectors={0x10,0x10,0x10}};assert(mhc_enter(&s,&o,0x1000));assert(mhc_leave(&s,&o));assert(!m.roots);
 m=(struct mock){.root=0x2000,.selectors={0x33,0x33,0x33}};assert(!mhc_enter(&s,&o,0x1000));assert(!m.roots&&!m.sets);
 m.selectors.ss=0;assert(!mhc_enter(&s,&o,0x1001));assert(!m.roots);
 assert(mhc_enter(&s,&o,0x1000));m.root=0x3000;assert(!mhc_leave(&s,&o));assert(s.entered&&m.root==0x3000);
 m.root=0x1000;m.selectors.ss=0;assert(!mhc_leave(&s,&o));assert(s.entered);
 m.selectors.ss=0x10;assert(mhc_leave(&s,&o));
 puts("managed host context: strict kernel-root proof, exact caller restore, SS normalization, fail-closed mismatch PASS (host model)");
}
