#include "../domain_typed_alias_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t ram[8][512];
static struct domain_typed_alias_workspace workspace;
static struct domain_typed_alias_plan plan;
static const struct domain_typed_root_binding roots[]={
    {1,1,0x101e,3},{2,2,0x501e,0}};
static struct domain_typed_request request={roots,2,48,0x200000,32};
static bool resolve(void *ctx,uint64_t hpa,const uint64_t **view) {
    (void)ctx;
    if(hpa<0x1000||hpa>0x8000||(hpa&4095))return false;
    *view=ram[hpa/4096-1];return true;
}
static void init(void) {
    memset(ram,0,sizeof ram);memset(&plan,0,sizeof plan);
    for(unsigned root=0;root<2;root++) {
        for(unsigned i=0;i<3;i++)ram[root*4+i][0]=((uint64_t)(root*4+i+2)*4096)|7;
        ram[root*4+3][8]=0x300033;
    }
    ram[3][15]=0x200073;ram[3][511]=0x200071;
}
static bool prepare(unsigned source,unsigned dest,uint64_t epoch) {
    return domain_typed_alias_prepare(&plan,&request,source,dest,epoch,resolve,NULL,&workspace);
}
int main(void) {
    init();assert(prepare(6,0,7));
    assert(plan.before.alias_count==2&&plan.detached.alias_count==0);
    assert(plan.before.roots[1].aliases==0&&plan.before.roots[1].binding.historical_mask==0);
    assert(domain_typed_alias_detach(&plan,7,&workspace));
    assert(ram[3][15]==0&&ram[3][511]==0&&ram[3][8]==0x300033);
    assert(domain_typed_alias_publish(&plan,7,&workspace));
    assert(ram[3][15]==0x200043&&ram[3][511]==0x200041);
    memset(&plan,0,sizeof plan);
    assert(prepare(0,6,8)); /* actual UC source, not stale initial-WB raw */
    assert(domain_typed_alias_detach(&plan,8,&workspace));
    assert(domain_typed_alias_publish(&plan,8,&workspace));
    assert(ram[3][15]==0x200073&&ram[3][511]==0x200071);
    init();assert(!prepare(0,6,7)&&plan.phase==DTA_FAILED);
    assert(ram[3][15]==0x200073);
    init();assert(prepare(6,0,7));
    ram[7][510]=0x444033; /* zero-alias spare root is still frozen */
    assert(!domain_typed_alias_detach(&plan,7,&workspace));
    assert(ram[3][15]==0x200073&&ram[3][511]==0x200071);
    init();assert(prepare(6,0,7));
    assert(!domain_typed_alias_detach(&plan,8,&workspace));
    assert(!domain_typed_alias_detach(&plan,7,&workspace)); /* no partial retry */
    init();assert(prepare(6,0,7));
    assert(domain_typed_alias_detach(&plan,7,&workspace));
    ram[3][511]=0x200073;
    assert(!domain_typed_alias_publish(&plan,7,&workspace));
    assert(ram[3][15]==0); /* prevalidate whole list before first store */
    init();assert(prepare(6,0,7));
    assert(domain_typed_alias_detach(&plan,7,&workspace));
    ram[7][510]=0x444033;
    assert(!domain_typed_alias_publish(&plan,7,&workspace)&&ram[3][15]==0);
    init();assert(prepare(6,0,7));
    assert(domain_typed_alias_detach(&plan,7,&workspace));
    ram[1][1]=0x1007; /* new branch/cycle */
    assert(!domain_typed_alias_publish(&plan,7,&workspace));
    assert(ram[3][15]==0&&ram[3][511]==0);
    init();ram[3][15]=0;ram[3][511]=0;
    assert(!prepare(6,0,7));
    init();assert(!prepare(6,4,9));
    init();assert(domain_typed_alias_prepare_type(&plan,&request,6,4,9,4,resolve,NULL,&workspace));
    assert(domain_typed_alias_detach(&plan,9,&workspace));
    assert(domain_typed_alias_publish(&plan,9,&workspace));
    assert(ram[3][15]==0x200063&&ram[3][511]==0x200061);
    memset(&plan,0,sizeof plan);
    assert(!domain_typed_alias_prepare_type(&plan,&request,6,4,10,4,resolve,NULL,&workspace)); /* stale WB source */
    memset(&plan,0,sizeof plan);
    assert(domain_typed_alias_prepare_type(&plan,&request,4,6,10,4,resolve,NULL,&workspace));
    assert(domain_typed_alias_detach(&plan,10,&workspace));
    assert(domain_typed_alias_publish(&plan,10,&workspace));
    assert(ram[3][15]==0x200073&&ram[3][511]==0x200071);
    init();assert(!domain_typed_alias_prepare_type(&plan,&request,6,0,1,4,resolve,NULL,&workspace));
    init();assert(!domain_typed_alias_prepare_type(&plan,&request,0,4,1,4,resolve,NULL,&workspace));
    init();assert(!domain_typed_alias_prepare_type(&plan,&request,6,4,1,1,resolve,NULL,&workspace));
    puts("PASS stopped-root alias WB/UC roundtrip, full-digest checks, epoch and no-partial publication");
}
