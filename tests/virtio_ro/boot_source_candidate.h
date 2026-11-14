#ifndef SYLE_BOOT_SOURCE_CANDIDATE_H
#define SYLE_BOOT_SOURCE_CANDIDATE_H
#include "linux_boot.h"
/* Trusted producer classification, NOT a caller capability or native admission.
 * Generated local sources are consumed synchronously, never retained. */
enum cs_source {CS_IMAGE,CS_INITRD,CS_CMD,CS_PARAMS,CS_ENTRY};
struct cs_writer {void *context;bool (*write)(void*,enum cs_source,uint64_t,const void*,size_t);bool (*zero)(void*,uint64_t,size_t);};
bool cs_lb_inspect(const void*,size_t,size_t,const char*,struct lb_plan*);
bool cs_lb_load(const void*,size_t,const void*,size_t,const char*,const struct cs_writer*,struct lb_plan*);
bool cs_lb_entry_prepare(const struct lb_plan*,const struct cs_writer*,struct lb_entry_state*);
#endif
