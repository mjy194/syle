#include "lgm_audit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct linux_guest_memory m;
static uint64_t tables[LGM_TABLE_COUNT][512];
static uint64_t fail_map,fail_wb;
static void *map(void *c,uint64_t p){(void)c;if(p==fail_map)return 0;for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(m.tables[i]==p)return tables[i];return 0;}
static bool wb(void *c,uint64_t p){(void)c;return p!=fail_wb;}
static void init(void){memset(&m,0,sizeof(m));memset(tables,0,sizeof(tables));fail_map=fail_wb=0;m.owner.map=map;m.owner.writeback=wb;for(unsigned i=0;i<LGM_PAGE_COUNT;i++)m.pages[i]=0x1000000+(uint64_t)i*4096;for(unsigned i=0;i<LGM_TABLE_COUNT;i++)m.tables[i]=0x100000+(uint64_t)i*4096;tables[0][0]=m.tables[1]|7;tables[1][0]=m.tables[2]|7;for(unsigned i=0;i<LGM_PT_COUNT;i++){tables[2][i]=m.tables[3+i]|7;for(unsigned j=0;j<512;j++)tables[3+i][j]=m.pages[i*512+j]|0x37;}m.eptp=m.tables[0]|0x1e;m.prepared=true;}
int main(void){uint64_t token=123;init();assert(vro_lgm_audit_page(&m,4095,true,&token)&&token==m.pages[0]);assert(vro_lgm_audit_page(&m,LGM_RAM_SIZE-1,true,&token)&&token==m.pages[LGM_PAGE_COUNT-1]);
 init();m.pages[100]=m.pages[0];token=123;assert(!vro_lgm_audit_page(&m,0,true,&token)&&token==123);
 init();m.pages[100]=m.tables[20];assert(!vro_lgm_audit_page(&m,0,true,&token));
 init();tables[3][0]=m.pages[1]|0x37;assert(!vro_lgm_audit_page(&m,0,true,&token));
 init();tables[3][0]&=~UINT64_C(2);assert(!vro_lgm_audit_page(&m,0,true,&token));assert(vro_lgm_audit_page(&m,0,false,&token));
 init();tables[2][0]|=0x80;assert(!vro_lgm_audit_page(&m,0,true,&token));
 for(unsigned i=0;i<4;i++){init();fail_map=m.tables[i];assert(!vro_lgm_audit_page(&m,0,true,&token));init();fail_wb=m.tables[i];assert(!vro_lgm_audit_page(&m,0,true,&token));}
 init();fail_wb=m.pages[0];assert(!vro_lgm_audit_page(&m,0,true,&token));
 init();m.prepared=false;assert(!vro_lgm_audit_page(&m,0,true,&token));init();m.eptp+=4096;assert(!vro_lgm_audit_page(&m,0,true,&token));init();assert(!vro_lgm_audit_page(&m,LGM_RAM_SIZE,true,&token));
 /* Scope regression: these PASS results are NOT global EPT safety evidence. */
 init();tables[3][1]=m.pages[0]|0x37;assert(m.pages[1]!=m.pages[0]);assert(vro_lgm_audit_page(&m,0,true,&token));
 init();m.pages[100]=0;assert(vro_lgm_audit_page(&m,0,true,&token));
 puts("LGM ledger/EPT read-only audit: PASS (not DMA/ownership proof)");return 0;
}
