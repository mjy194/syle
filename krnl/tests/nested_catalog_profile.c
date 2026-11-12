#define main old_domain_profile_main
#include "nested_domain_profile.c"
#undef main
static bool catalog_allowed(void *context,uint64_t key){
 const uint64_t *keys=context;return key==keys[0]||key==keys[1]||key==keys[2]||key==keys[3];
}
int main(void){
 struct nested_profile p={.cr0=0x80010033,.cr4=0x2020,.efer=0x500,.pat=0x0007040600070406,
  .l2_entry=0x6100,.l2_resume=0x6103,.l1_exit=0x6200,.nested_ept=true,.shared_domain=true,
  .domain_contexts=true,.domain_eviction=true,.domain_typed_uc=true,.domain_typed_contexts=true,.domain_typed_dynamic=true};
 uint64_t keys[]={0x3101e,0x5701e,0x8b01e,0xa001e};
 p.domain_initial_eptp=keys[0];p.domain_root_allowed=catalog_allowed;p.domain_root_context=keys;
 struct nested_field_value table[NVMX_FIELDS];size_t count=nested_profile_table(&p,table,NVMX_FIELDS);assert(count);
 struct nvmx s;assert(nvmx_init(&s,0xd000,0xe000,48));
 assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_ON,.operand=0xd000,.region_revision=NVMX_REVISION,.flags=2}).status==NVMX_SUCCESS);
 assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_CLEAR,.operand=0xe000,.region_revision=NVMX_REVISION,.flags=2}).status==NVMX_SUCCESS);
 assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_PTRLD,.operand=0xe000,.region_revision=NVMX_REVISION,.flags=2}).status==NVMX_SUCCESS);
 for(size_t i=0;i<count;i++)assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_WRITE,.field=table[i].field,.value=table[i].value,.flags=2}).status==NVMX_SUCCESS);
 uint64_t cases[]={keys[0],keys[1],keys[2],keys[3],0x1001e,0x1401e,0x1801e,keys[0]|0x40,keys[0]^6};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_WRITE,.field=0x201a,.value=cases[i],.flags=2}).status==NVMX_SUCCESS);
  assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_LAUNCH,.flags=2}).status==NVMX_ENTRY_REQUEST);
  assert(nested_profile_check(&s,&p)==(i<4?NP_VALID:NP_CONTROLS));assert(nvmx_entry_cancel(&s));
 }
 puts("PASS N40B production profile admits four arbitrary catalog keys and rejects old hardcoded/config-changed keys");
}
