#include "../domain_typed_inventory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t ram[12][512];
static struct domain_typed_workspace work;
static struct domain_typed_inventory out,before;
static bool missing;
static bool resolve(void *ctx,uint64_t hpa,const uint64_t **view) {
    (void)ctx;
    if(missing||hpa<0x1000||hpa>0xc000||(hpa&4095))return false;
    *view=ram[hpa/4096-1];return true;
}
static void init(void) {
    memset(ram,0,sizeof ram);
    ram[0][0]=0x2007;ram[1][0]=0x3007;ram[2][0]=0x4007;
    ram[3][15]=0x200073; /* WB IPAT1 RW target */
    ram[3][6]=0x300075; /* non-target WB RX */
    ram[0][1]=0x5007;ram[4][7]=0x6007;ram[5][9]=0x7007;
    ram[6][511]=0x200043; /* distant UC alias, arbitrary indices */
    ram[7][0]=0x9007;ram[8][0]=0xa007;ram[9][0]=0xb007;
    ram[10][8]=0x400033; /* historical root without target */
    memset(&out,0xa5,sizeof out);before=out;missing=false;
}
static struct domain_typed_root_binding roots[]={{1,3,0x101e,3},{2,4,0x801e,2}};
static struct domain_typed_request request={roots,2,48,0x200000,64};
static void reject(enum domain_typed_status status) {
    assert(domain_typed_inventory_audit(&request,resolve,NULL,&work,&out)==status);
    assert(!memcmp(&out,&before,sizeof out));
}
int main(void) {
    init();
    assert(domain_typed_inventory_audit(&request,resolve,NULL,&work,&out)==DOMAIN_TYPED_OK);
    assert(out.total_nodes==11&&out.root_count==2&&out.alias_count==2);
    assert(out.roots[0].visited_nodes==7&&out.roots[1].visited_nodes==4);
    assert(out.roots[1].aliases==0&&out.roots[1].binding.historical_mask==2);
    assert(out.aliases[0].gpa==0xf000&&out.aliases[0].memory_type==6);
    assert(out.aliases[1].gpa==((1ull<<39)|(7ull<<30)|(9ull<<21)|(511ull<<12)));
    assert(out.aliases[1].memory_type==0&&out.aliases[1].permissions==3);
    uint64_t digest0=out.roots[0].scan_digest,digest1=out.roots[1].scan_digest;
    assert(digest0&&digest1);
    ram[10][510]=0x440033; /* no new target alias, still changes historical-root digest */
    assert(domain_typed_inventory_audit(&request,resolve,NULL,&work,&out)==DOMAIN_TYPED_OK);
    assert(out.roots[0].scan_digest==digest0&&out.roots[1].scan_digest!=digest1);
    init();ram[0][511]=0xc007;ram[11][511]=0xc007;reject(DOMAIN_TYPED_OWNERSHIP);
    init();request.node_budget=10;reject(DOMAIN_TYPED_BUDGET);request.node_budget=64;
    init();missing=true;reject(DOMAIN_TYPED_OWNERSHIP);
    init();ram[2][8]=0x2000b7;reject(DOMAIN_TYPED_UNSUPPORTED); /* huge */
    init();ram[3][15]&=~0x40ull;reject(DOMAIN_TYPED_UNSUPPORTED);
    init();ram[3][15]|=4;reject(DOMAIN_TYPED_UNSUPPORTED);
    init();ram[3][6]=0x300003;reject(DOMAIN_TYPED_UNSUPPORTED); /* unrelated UC */
    init();ram[3][15]|=0x100;reject(DOMAIN_TYPED_UNSUPPORTED); /* AD bits not ignored */
    init();ram[3][15]|=1ull<<48;reject(DOMAIN_TYPED_UNSUPPORTED);
    init();ram[0][400]=request.target_hpa|7;reject(DOMAIN_TYPED_OWNERSHIP);
    init();roots[1].id=1;reject(DOMAIN_TYPED_INVALID);roots[1].id=2;
    init();roots[0].eptp=0x105e;reject(DOMAIN_TYPED_INVALID);roots[0].eptp=0x101e;
    init();for(unsigned n=0;n<17;n++)ram[3][n]=0x200073;
    reject(DOMAIN_TYPED_CAPACITY);
    init();ram[0][1]=0;ram[3][15]=0;
    assert(domain_typed_inventory_audit(&request,resolve,NULL,&work,&out)==DOMAIN_TYPED_OK);
    assert(out.alias_count==0&&out.total_nodes==8);
    init();roots[1].historical_mask=0;
    assert(domain_typed_inventory_audit(&request,resolve,NULL,&work,&out)==DOMAIN_TYPED_OK);
    assert(out.roots[1].binding.historical_mask==0&&out.roots[1].visited_nodes==4);
    roots[1].historical_mask=2;
    init();ram[0][0]&=~4ull;ram[3][15]|=4;reject(DOMAIN_TYPED_UNSUPPORTED);
    init();ram[3][15]=0x200063;ram[6][511]=0x200063;
    reject(DOMAIN_TYPED_UNSUPPORTED); /* legacy UC rejects WT */
    assert(domain_typed_inventory_audit_type(&request,4,resolve,NULL,&work,&out)==DOMAIN_TYPED_OK);
    assert(out.aliases[0].memory_type==4&&out.aliases[1].memory_type==4);
    init();assert(domain_typed_inventory_audit_type(&request,4,resolve,NULL,&work,&out)==DOMAIN_TYPED_UNSUPPORTED); /* UC alias */
    assert(domain_typed_inventory_audit_type(&request,1,resolve,NULL,&work,&out)==DOMAIN_TYPED_INVALID);
    puts("PASS exhaustive typed inventory, distant aliases, historical zero-alias roots, failure atomicity");
}
