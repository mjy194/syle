#ifndef SYLE_NESTED_EPT_AD_HW_H
#define SYLE_NESTED_EPT_AD_HW_H
#include "nested_ept_sparse_hw.h"
#include "nested_ept_ad.h"
enum nested_ad_action {NESTED_AD_RETRY,NESTED_AD_L1_DENIED,NESTED_AD_FATAL};
struct nested_ad_record;
struct nested_ept_ad_cache {struct nested_ad_record *records;struct nested_ept_policy policy;};
void nested_ept_ad_cache_clear(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *);
enum nested_ad_action nested_ept_ad_tree_handle(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *,struct nested_ept_sparse *,uint64_t,uint64_t,uint8_t,struct nested_ept_result *);
bool nested_ept_ad_hw_init(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *,struct ept_fixture *,unsigned);
enum nested_ad_action nested_ept_ad_hw_handle(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *,uint64_t,uint8_t,struct nested_ept_result *);
bool nested_ept_ad_hw_fault(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *,uint64_t,uint8_t);
bool nested_ept_ad_hw_reset(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *);
void nested_ept_ad_hw_destroy(struct nested_ept_ad_cache *,struct nested_ept_sparse_hw *);
#endif
