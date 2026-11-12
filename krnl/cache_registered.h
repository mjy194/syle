#ifndef SYLE_CACHE_REGISTERED_H
#define SYLE_CACHE_REGISTERED_H
#include "../include/syle/boot.h"
#include "ept.h"
#include "nested_ept_contexts.h"
#include "nested_ept.h"
bool cache_registered_platform_type(const struct syle_boot_info *,uint8_t);
bool cache_registered_platform(const struct syle_boot_info *);
/* One private fixture; single tree or serialized registered contexts. No guest executes in begin/end. */
void cache_registered_begin(struct ept_fixture *);
void cache_registered_pin(void);
void cache_registered_unpin_after_destroy(void);
void cache_registered_finish(struct ept_fixture *);
bool cache_registered_admit(void *,const struct nested_ept_result *);
bool cache_registered_resolve(void *,uint64_t,uint8_t,uint64_t *,const uint64_t **,uint8_t *);
void cache_registered_context_lifecycle(void *,struct nested_ept_context_item *,enum nested_context_event);
uint64_t cache_registered_context_refs(void);
bool cache_registered_pat_entry(uint64_t pat,uint64_t cr0);
#endif
