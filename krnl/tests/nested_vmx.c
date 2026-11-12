#include "../nested_vmx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct nvmx_result call(struct nvmx *s,enum nvmx_op op,uint64_t operand,uint64_t field,uint64_t value) {
    return nvmx_execute(s,(struct nvmx_request){op,operand,field,value,UINT64_C(0x202)|0x8d5,NVMX_REVISION});
}
static void ok(struct nvmx_result r) {assert(r.status==NVMX_SUCCESS);assert((r.flags&0x8d5)==0);assert(r.flags&0x202);}
static void err(struct nvmx_result r,unsigned error) {assert(r.status==NVMX_FAIL_VALID&&r.error==error);assert((r.flags&0x8d5)==0x40);}
int main(void) {
    struct nvmx s;
    assert(!nvmx_init(&s,0x1001,0x2000,48));
    assert(!nvmx_init(&s,0x1000,0x1000,48));
    assert(!nvmx_init(&s,0x1000,UINT64_C(1)<<48,48));
    assert(nvmx_init(&s,0x1000,0x2000,48));
    assert(call(&s,NVMX_READ,0,0x681e,0).status==NVMX_FAULT_UD);
    struct nvmx_result r=call(&s,NVMX_ON,0x1001,0,0);
    assert(r.status==NVMX_FAIL_INVALID&&(r.flags&0x8d5)==1&&!s.root);
    ok(call(&s,NVMX_ON,0x1000,0,0));
    ok(nvmx_invept_result(&s,true,2));
    assert(nvmx_invept_result(&s,false,2).status==NVMX_FAIL_INVALID);
    assert(call(&s,NVMX_ON,0x1000,0,0).status==NVMX_FAIL_INVALID);
    r=call(&s,NVMX_PTRST,0,0,0);ok(r);assert(r.value==UINT64_MAX);
    assert(call(&s,NVMX_PTRLD,0x2000,0,0).status==NVMX_UNSUPPORTED); /* must clear */
    ok(call(&s,NVMX_CLEAR,0x2000,0,0));ok(call(&s,NVMX_PTRLD,0x2000,0,0));
    err(call(&s,NVMX_ON,0x1000,0,0),15);
    err(nvmx_invept_result(&s,false,2),28);
    err(call(&s,NVMX_CLEAR,0x2001,0,0),2);
    err(call(&s,NVMX_CLEAR,0x1000,0,0),3);
    err(call(&s,NVMX_PTRLD,0x2001,0,0),9);
    err(call(&s,NVMX_PTRLD,0x1000,0,0),10);
    struct nvmx_request bad={.op=NVMX_PTRLD,.operand=0x2000,.region_revision=0};
    err(nvmx_execute(&s,bad),11);
    err(call(&s,NVMX_WRITE,0,0x4400,22),13);
    r=call(&s,NVMX_READ,0,0x4400,0);ok(r);assert(r.value==13); /* success preserves error */
    err(call(&s,NVMX_READ,0,0x10000,0),12);
    err(call(&s,NVMX_READ,0,0x0801,0),12);
    err(call(&s,NVMX_WRITE,0,0x4001,0),12);
    err(call(&s,NVMX_READ,0,0x681f,0),12);
    struct nvmx before=s;
    r=call(&s,NVMX_WRITE,0,0x0000,123); /* real VPID omitted by profile */
    assert(r.status==NVMX_UNSUPPORTED&&memcmp(&before,&s,sizeof s)==0);
    ok(call(&s,NVMX_WRITE,0,0x800,UINT64_MAX));assert(call(&s,NVMX_READ,0,0x800,0).value==0xffff);
    ok(call(&s,NVMX_WRITE,0,0x4000,UINT64_MAX));assert(call(&s,NVMX_READ,0,0x4000,0).value==UINT32_MAX);
    ok(call(&s,NVMX_WRITE,0,0x681e,UINT64_MAX));assert(call(&s,NVMX_READ,0,0x681e,0).value==UINT64_MAX);
    ok(call(&s,NVMX_WRITE,0,0x2806,UINT64_C(0x1122334455667788)));
    assert(call(&s,NVMX_READ,0,0x2807,0).value==0x11223344);
    ok(call(&s,NVMX_WRITE,0,0x2807,UINT64_C(0xffffaabbccdd)));
    assert(call(&s,NVMX_READ,0,0x2806,0).value==UINT64_C(0xaabbccdd55667788));
    err(call(&s,NVMX_RESUME,0,0,0),5);
    r=call(&s,NVMX_LAUNCH,0,0,0);assert(r.status==NVMX_ENTRY_REQUEST&&!s.launched&&s.prepared);
    assert(r.flags==(UINT64_C(0x202)|0x8d5)); /* no fake success flags */
    before=s;assert(call(&s,NVMX_OFF,0,0,0).status==NVMX_UNSUPPORTED&&!memcmp(&before,&s,sizeof s));
    assert(nvmx_entry_fail(&s,33,0x202).status==NVMX_UNSUPPORTED&&s.prepared);
    err(nvmx_entry_fail(&s,7,0x202),7);assert(!s.prepared&&!s.launched);
    assert(call(&s,NVMX_LAUNCH,0,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_commit(&s)&&s.launched&&s.running);
    uint64_t sf[]={0x681e,0x6c02},sv[]={0xdead,0xbeef};
    before=s;assert(!nvmx_exit_snapshot(&s,sf,sv,2)&&!memcmp(&s,&before,sizeof s));
    sf[1]=0x681c;assert(nvmx_exit_snapshot(&s,sf,sv,2));
    assert(nvmx_exit_store(&s,0x4402,18));
    assert(nvmx_exit_store(&s,0x681e,0x6123));
    assert(!nvmx_exit_store(&s,0x6c02,0xdead)); /* never virtual host fields */
    assert(!nvmx_exit_store(&s,0x4002,1));
    uint64_t saved;assert(nvmx_peek(&s,0x4402,&saved)&&saved==18);
    assert(!nvmx_entry_commit(&s));assert(call(&s,NVMX_OFF,0,0,0).status==NVMX_UNSUPPORTED);
    assert(nvmx_return_from_l2(&s));assert(!nvmx_exit_store(&s,0x4402,19));err(call(&s,NVMX_LAUNCH,0,0,0),4);
    assert(call(&s,NVMX_RESUME,0,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_cancel(&s)&&s.launched);
    ok(call(&s,NVMX_OFF,0,0,0));assert(s.launched&&s.poisoned);
    ok(call(&s,NVMX_ON,0x1000,0,0));
    assert(call(&s,NVMX_PTRLD,0x2000,0,0).status==NVMX_UNSUPPORTED);
    ok(call(&s,NVMX_CLEAR,0x2000,0,0));assert(!s.launched&&!s.poisoned);
    ok(call(&s,NVMX_PTRLD,0x2000,0,0));ok(call(&s,NVMX_CLEAR,0x2000,0,0));
    assert(call(&s,NVMX_PTRST,0,0,0).value==NVMX_NONE);
    ok(call(&s,NVMX_OFF,0,0,0));assert(!s.poisoned);
    puts("PASS N0 software VMX state/flags/error/VMCS12/entry transaction; no hardware nesting");
}
