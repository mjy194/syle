#include "nested_vmx.h"
struct field {uint16_t encoding;uint8_t width;bool ro;};
#define F(e,w) {e,w,false}
#define R(e,w) {e,w,true}
/* Explicit experimental VMCS12 contract; omitted real architectural fields
 * report UNSUPPORTED, not a false claim that the processor lacks them. */
static const struct field fields[]={
 F(0x800,16),F(0x802,16),F(0x804,16),F(0x806,16),F(0x808,16),F(0x80a,16),F(0x80c,16),F(0x80e,16),
 F(0xc00,16),F(0xc02,16),F(0xc04,16),F(0xc06,16),F(0xc08,16),F(0xc0a,16),F(0xc0c,16),
 F(0x201a,64),R(0x2400,64),F(0x2800,64),F(0x2802,64),F(0x2804,64),F(0x2806,64),F(0x2c00,64),F(0x2c02,64),
 F(0x4000,32),F(0x4002,32),F(0x4004,32),F(0x4006,32),F(0x4008,32),F(0x400a,32),
 F(0x400c,32),F(0x400e,32),F(0x4010,32),F(0x4012,32),F(0x4014,32),
 F(0x4016,32),F(0x4018,32),F(0x401a,32),F(0x401e,32),
 R(0x4400,32),R(0x4402,32),R(0x4404,32),R(0x4406,32),R(0x4408,32),R(0x440a,32),R(0x440c,32),R(0x440e,32),
 F(0x4800,32),F(0x4802,32),F(0x4804,32),F(0x4806,32),F(0x4808,32),F(0x480a,32),F(0x480c,32),F(0x480e,32),F(0x4810,32),F(0x4812,32),
 F(0x4814,32),F(0x4816,32),F(0x4818,32),F(0x481a,32),F(0x481c,32),F(0x481e,32),F(0x4820,32),F(0x4822,32),F(0x4824,32),F(0x4826,32),F(0x482a,32),F(0x4c00,32),
 F(0x6000,0),F(0x6002,0),F(0x6004,0),F(0x6006,0),
 R(0x6400,0),R(0x640a,0),
 F(0x6800,0),F(0x6802,0),F(0x6804,0),F(0x6806,0),F(0x6808,0),F(0x680a,0),F(0x680c,0),F(0x680e,0),F(0x6810,0),F(0x6812,0),F(0x6814,0),F(0x6816,0),F(0x6818,0),F(0x681a,0),F(0x681c,0),F(0x681e,0),F(0x6820,0),F(0x6822,0),F(0x6824,0),F(0x6826,0),
 F(0x6c00,0),F(0x6c02,0),F(0x6c04,0),F(0x6c06,0),F(0x6c08,0),F(0x6c0a,0),F(0x6c0c,0),F(0x6c0e,0),F(0x6c10,0),F(0x6c12,0),F(0x6c14,0),F(0x6c16,0)
};
#define COUNT (sizeof(fields)/sizeof(fields[0]))
_Static_assert(COUNT<=NVMX_FIELDS,"VMCS12 storage");
static int find(uint64_t field,bool *high) {
    for(unsigned i=0;i<COUNT;i++) {
        if(field==fields[i].encoding) {*high=false;return (int)i;}
        if(fields[i].width==64&&field==(uint64_t)fields[i].encoding+1) {*high=true;return (int)i;}
    }
    return -1;
}
static bool address(uint64_t p,unsigned bits) {
    return !(p&4095)&&!(p>>bits);
}
static struct nvmx_result result(struct nvmx *s,enum nvmx_status status,uint32_t error,uint64_t flags,uint64_t value) {
    if(status==NVMX_FAIL_VALID&&s->current==NVMX_NONE) status=NVMX_FAIL_INVALID;
    if(status==NVMX_SUCCESS||status==NVMX_FAIL_VALID||status==NVMX_FAIL_INVALID) {
        flags&=~UINT64_C(0x8d5); /* OF SF ZF AF PF CF */
        if(status==NVMX_FAIL_INVALID) flags|=1;
        if(status==NVMX_FAIL_VALID) {
            bool high=false;int i=find(0x4400,&high);
            s->fields[i]=error;flags|=1u<<6;
        }
    }
    return (struct nvmx_result){status,error,flags,value};
}
bool nvmx_init(struct nvmx *s,uint64_t on,uint64_t vmcs,unsigned bits) {
    if(!s||bits<32||bits>52||on==vmcs||!address(on,bits)||!address(vmcs,bits)) return false;
    *s=(struct nvmx){.on_region=on,.vmcs_region=vmcs,.current=NVMX_NONE,.physical_bits=bits};
    return true;
}
struct nvmx_result nvmx_execute(struct nvmx *s,struct nvmx_request r) {
#define RET(st,err,val) return result(s,st,err,r.flags,val)
    if(s->prepared||s->running) {RET(NVMX_UNSUPPORTED,0,0);}
    if((unsigned)r.op>NVMX_RESUME) {RET(NVMX_UNSUPPORTED,0,0);}
    if(r.op==NVMX_ON) {
        if(s->root) {RET(NVMX_FAIL_VALID,15,0);}
        if(!address(r.operand,s->physical_bits)||r.region_revision!=NVMX_REVISION) {RET(NVMX_FAIL_INVALID,0,0);}
        if(r.operand!=s->on_region) {RET(NVMX_UNSUPPORTED,0,0);}
        s->root=true;s->current=NVMX_NONE;RET(NVMX_SUCCESS,0,0);
    }
    if(!s->root) {RET(NVMX_FAULT_UD,0,0);}
    if(r.op==NVMX_OFF) {
        if(s->active) s->poisoned=true;
        s->root=false;s->current=NVMX_NONE;RET(NVMX_SUCCESS,0,0);
    }
    if(r.op==NVMX_PTRST) {RET(NVMX_SUCCESS,0,s->current);}
    if(r.op==NVMX_PTRLD||r.op==NVMX_CLEAR) {
        bool clear=r.op==NVMX_CLEAR;
        if(!address(r.operand,s->physical_bits)) {RET(NVMX_FAIL_VALID,clear?2:9,0);}
        if(r.operand==s->on_region) {RET(NVMX_FAIL_VALID,clear?3:10,0);}
        if(r.operand!=s->vmcs_region) {RET(NVMX_UNSUPPORTED,0,0);}
        if(!clear&&r.region_revision!=NVMX_REVISION) {RET(NVMX_FAIL_VALID,11,0);}
        if(clear) {
            s->launched=false;s->initialized=true;s->active=false;s->poisoned=false;
            if(s->current==r.operand)s->current=NVMX_NONE;
        } else {
            if(!s->initialized||s->poisoned) {RET(NVMX_UNSUPPORTED,0,0);}
            s->current=r.operand;s->active=true;
        }
        RET(NVMX_SUCCESS,0,0);
    }
    if(s->current==NVMX_NONE) {RET(NVMX_FAIL_INVALID,0,0);}
    if(r.op==NVMX_LAUNCH||r.op==NVMX_RESUME) {
        if(r.op==NVMX_LAUNCH&&s->launched) {RET(NVMX_FAIL_VALID,4,0);}
        if(r.op==NVMX_RESUME&&!s->launched) {RET(NVMX_FAIL_VALID,5,0);}
        s->prepared=true;RET(NVMX_ENTRY_REQUEST,0,0);
    }
    bool high=false;int i=find(r.field,&high);
    /* Encodings with reserved bits cannot name an architectural component. */
    if((r.field&~UINT64_C(0x6fff))||
       ((r.field&1)&&((r.field>>13)&3)!=1)) {RET(NVMX_FAIL_VALID,12,0);}
    if(i<0) {RET(NVMX_UNSUPPORTED,0,0);}
    if(r.op==NVMX_WRITE) {
        if(fields[i].ro) {RET(NVMX_FAIL_VALID,13,0);}
        if(high) s->fields[i]=(s->fields[i]&UINT32_MAX)|((uint64_t)(uint32_t)r.value<<32);
        else if(fields[i].width==16) s->fields[i]=(uint16_t)r.value;
        else if(fields[i].width==32) s->fields[i]=(uint32_t)r.value;
        else s->fields[i]=r.value;
        RET(NVMX_SUCCESS,0,0);
    }
    RET(NVMX_SUCCESS,0,high?s->fields[i]>>32:s->fields[i]);
#undef RET
}
bool nvmx_entry_commit(struct nvmx *s) {
    if(!s->prepared||!s->root||s->current==NVMX_NONE||s->running) return false;
    s->prepared=false;s->running=true;s->launched=true;return true;
}
bool nvmx_entry_cancel(struct nvmx *s) {
    if(!s->prepared||s->running) return false;
    s->prepared=false;return true;
}
struct nvmx_result nvmx_entry_fail(struct nvmx *s,uint32_t error,uint64_t flags) {
    if(!s->prepared||s->running||(error!=7&&error!=8)) return result(s,NVMX_UNSUPPORTED,0,flags,0);
    s->prepared=false;return result(s,NVMX_FAIL_VALID,error,flags,0);
}
bool nvmx_return_from_l2(struct nvmx *s) {
    if(!s||!s->running) return false;
    /* SDM 28.2: only a NORMAL virtual VM-exit clears entry-valid, preserving
     * all other bits. Internal L0 exits and failed entries do not call here. */
    bool high=false;int i=find(0x4016,&high);
    s->fields[i]&=~UINT64_C(0x80000000);
    s->running=false;return true;
}

bool nvmx_peek(const struct nvmx *s,uint64_t field,uint64_t *value) {
    bool high=false;int i=find(field,&high);
    if(!s||!value||i<0) return false;
    *value=high?s->fields[i]>>32:s->fields[i];return true;
}

bool nvmx_exit_store(struct nvmx *s,uint64_t field,uint64_t value) {
    bool high=false;int i=find(field,&high);
    if(!s||!s->running||i<0||high||(!fields[i].ro&&((field>>10)&3)!=2)) return false;
    if(fields[i].width==16)value=(uint16_t)value;
    if(fields[i].width==32)value=(uint32_t)value;
    s->fields[i]=value;return true;
}

struct nvmx_result nvmx_invept_result(struct nvmx *s,bool valid,uint64_t flags) {
    if(!s->root) return result(s,NVMX_FAULT_UD,0,flags,0);
    if(s->prepared||s->running) return result(s,NVMX_UNSUPPORTED,0,flags,0);
    return result(s,valid?NVMX_SUCCESS:NVMX_FAIL_VALID,valid?0:28,flags,0);
}

bool nvmx_exit_snapshot(struct nvmx *s,const uint64_t *field,const uint64_t *value,unsigned count) {
    if(!s||!field||!value||count>NVMX_FIELDS)return false;
    struct nvmx next=*s;
    for(unsigned i=0;i<count;i++)if(!nvmx_exit_store(&next,field[i],value[i]))return false;
    *s=next;return true;
}

/* SDM 27.8: no guest save, no successful launch transition, only these
 * exit-info fields change for this exact invalid-guest-state case. */
bool nvmx_failed_entry_finish(struct nvmx *s,uint64_t reason,uint64_t qualification) {
    if(!s||!s->prepared||s->running||!s->root||s->current==NVMX_NONE||
       reason!=UINT64_C(0x80000021)||qualification)return false;
    bool high=false;int r=find(0x4402,&high),q=find(0x6400,&high);
    s->fields[r]=reason;s->fields[q]=qualification;s->prepared=false;return true;
}
