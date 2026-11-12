#ifndef SYLE_NESTED_VMX_H
#define SYLE_NESTED_VMX_H
#include <stdint.h>
#include <stdbool.h>
#define NVMX_REVISION UINT32_C(0x53594c)
#define NVMX_FIELDS 128
#define NVMX_NONE UINT64_MAX
/* Pure N0 model only. No instruction decoder, hardware VMCS or host pointers. */
enum nvmx_op { NVMX_ON,NVMX_OFF,NVMX_PTRLD,NVMX_PTRST,NVMX_CLEAR,
               NVMX_READ,NVMX_WRITE,NVMX_LAUNCH,NVMX_RESUME,NVMX_INVEPT };
enum nvmx_status { NVMX_SUCCESS,NVMX_FAIL_INVALID,NVMX_FAIL_VALID,
                   NVMX_FAULT_UD,NVMX_FAULT_GP,NVMX_UNSUPPORTED,NVMX_ENTRY_REQUEST };
struct nvmx_result { enum nvmx_status status; uint32_t error; uint64_t flags,value; };
struct nvmx_request {
    enum nvmx_op op;
    uint64_t operand,field,value,flags;
    uint32_t region_revision;
    /* Adapter must establish CPL/CR4/memory-fault ordering before calling. */
};
struct nvmx {
    uint64_t on_region,vmcs_region,current;
    uint64_t fields[NVMX_FIELDS];
    unsigned physical_bits;
    bool root,launched,prepared,running,initialized,active,poisoned;
};
bool nvmx_init(struct nvmx *s,uint64_t on_region,uint64_t vmcs_region,unsigned physical_bits);
struct nvmx_result nvmx_execute(struct nvmx *s,struct nvmx_request r);
/* Prepared entry is NOT successful VMLAUNCH. Only a validated N1 hardware
 * entry may commit it. Early VMfail uses 7/8; guest-state failure is a separate
 * future exit-reflection path and must not call this as VMfail. */
/* Monitor-only read of whitelisted model data, including during prepare. */
bool nvmx_peek(const struct nvmx *s,uint64_t field,uint64_t *value);
struct nvmx_result nvmx_invept_result(struct nvmx *s,bool valid,uint64_t flags);
bool nvmx_failed_entry_finish(struct nvmx *s,uint64_t reason,uint64_t qualification);
bool nvmx_entry_commit(struct nvmx *s);
bool nvmx_entry_cancel(struct nvmx *s);
struct nvmx_result nvmx_entry_fail(struct nvmx *s,uint32_t error,uint64_t flags);
/* N0 test/adapter lifecycle hook, not yet full architectural exit reflection. */
bool nvmx_exit_snapshot(struct nvmx *s,const uint64_t *fields,const uint64_t *values,unsigned count);
bool nvmx_exit_store(struct nvmx *s,uint64_t field,uint64_t value);
bool nvmx_return_from_l2(struct nvmx *s);
#endif
