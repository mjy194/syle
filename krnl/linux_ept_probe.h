#ifndef SYLE_LINUX_EPT_PROBE_H
#define SYLE_LINUX_EPT_PROBE_H
#include "linux_guest_manager.h"
#define LEP_CODE UINT64_C(0xa000)
#define LEP_DATA UINT64_C(0xb000)
#define LEP_EXEC UINT64_C(0xc000)
#define LEP_INITIAL UINT64_C(0x1122334455667788)
#define LEP_WRITTEN UINT64_C(0x7766554433221100)
#define LEP_EXEC_VALUE UINT64_C(0x123456789abcdef0)
struct lep_context {
 struct lgs_session *session;
 uint64_t *data_leaf,*exec_leaf,data_original,exec_original;
 uint64_t read_rip,read_done,write_rip,write_done,exec_done;
 unsigned phase,exits,faults;
 bool prepared,started,repaired,complete,published,failed;
};
bool lep_init(struct lep_context*,struct lgm_service_ops*);
bool lep_reset(void*);
bool lep_prepare(void*,struct lgs_session*);
bool lep_before(void*,struct lgs_session*);
bool lep_after(void*,struct lgs_session*,const struct lgs_entry_result*);
bool lep_dispatch(void*,struct lgs_session*,uint32_t);
int lep_recv(void*);
#endif
