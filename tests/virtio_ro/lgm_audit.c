#include "lgm_audit.h"
#define MASK UINT64_C(0x000ffffffffff000)
static bool physical(uint64_t p){return p&&!(p&~MASK);}
static const uint64_t *table(const struct linux_guest_memory *m,unsigned i){
 uint64_t p=m->tables[i];
 if(!physical(p)||!m->owner.writeback(m->owner.context,p))return 0;
 const uint64_t *v=m->owner.map(m->owner.context,p);
 return v&&!((uintptr_t)v&7)?v:0;
}
bool vro_lgm_audit_page(const struct linux_guest_memory *m,uint64_t gpa,bool write,uint64_t *token){
 if(!m||!token||!m->prepared||!m->owner.map||!m->owner.writeback||gpa>=LGM_RAM_SIZE||m->eptp!=(m->tables[0]|0x1e))return false;
 unsigned page=(unsigned)(gpa/4096),pt=page/512;
 uint64_t pa=m->pages[page];
 if(!physical(pa)||!m->owner.writeback(m->owner.context,pa))return false;
 /* Detect this token duplicated in the RAM ledger ONLY. Other actual EPT
  * leaves may alias it despite distinct ledger tokens; they are not walked.
  * Table tokens must be unique and cannot be present in the RAM ledger. */
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(i!=page&&m->pages[i]==pa)return false;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++){
  if(!physical(m->tables[i]))return false;
  for(unsigned j=0;j<i;j++)if(m->tables[j]==m->tables[i])return false;
  for(unsigned j=0;j<LGM_PAGE_COUNT;j++)if(m->pages[j]==m->tables[i])return false;
 }
 const uint64_t *p=table(m,0);if(!p||(p[0]&~UINT64_C(0x100))!=(m->tables[1]|7))return false;
 p=table(m,1);if(!p||(p[0]&~UINT64_C(0x100))!=(m->tables[2]|7))return false;
 p=table(m,2);if(!p||(p[pt]&~UINT64_C(0x100))!=(m->tables[3+pt]|7))return false;
 p=table(m,3+pt);if(!p)return false;
 uint64_t leaf=p[page%512];
 /* Exact LGM WB 4K identity topology, allowing accessed/dirty observations.
  * Not a general EPT walker or huge-page implementation. */
 if((leaf&MASK)!=pa||(leaf&~(MASK|UINT64_C(0x337)))||!(leaf&1)||(write&&!(leaf&2))||(leaf&0x38)!=0x30)return false;
 *token=pa;return true;
}
