#ifndef SYLE_NESTED_EPT_AD_CONTEXT_HW_H
#define SYLE_NESTED_EPT_AD_CONTEXT_HW_H
#include "nested_ept_contexts.h"
#include "nested_ept_ad_hw.h"
void nested_ad_context_lifecycle(void *,struct nested_ept_context_item *,enum nested_context_event);
enum nested_ad_action nested_ad_context_handle(struct nested_ept_sparse_hw *,struct nested_ept_context_item *,uint64_t,uint8_t,struct nested_ept_result *);
#endif
