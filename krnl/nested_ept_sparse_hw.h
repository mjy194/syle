#ifndef SYLE_NESTED_EPT_SPARSE_HW_H
#define SYLE_NESTED_EPT_SPARSE_HW_H
#include "nested_ept_sparse.h"
#include "ept.h"
struct nested_ept_sparse_hw {
    struct nested_ept_sparse tree;
    struct ept_fixture *fixture;
    uint64_t eptp;
    unsigned allocated_pages;
};
enum nested_sparse_fault { NS_FAULT_FILLED,NS_FAULT_L1,NS_FAULT_L0,NS_FAULT_UNSUPPORTED,NS_FAULT_MISCONFIG };
struct nested_sparse_ops nested_sparse_hw_ops(void);
bool nested_sparse_hw_resolve(void *,uint64_t,uint8_t,uint64_t *,const uint64_t **,uint8_t *);
enum nested_sparse_fault nested_sparse_hw_tree_fault(struct nested_ept_sparse_hw *,struct nested_ept_sparse *,uint64_t eptp12,uint64_t gpa,uint8_t access,struct nested_ept_result *);
enum nested_sparse_fault nested_sparse_hw_tree_fault_policy(struct nested_ept_sparse_hw *,struct nested_ept_sparse *,uint64_t,uint64_t,uint8_t,struct nested_ept_result *,const struct nested_ept_policy *);
enum nested_sparse_fault nested_sparse_hw_classified_fault(struct nested_ept_sparse_hw *,uint64_t,uint8_t,struct nested_ept_result *,const struct nested_ept_policy *);
bool nested_ept_sparse_hw_init(struct nested_ept_sparse_hw *,struct ept_fixture *,unsigned maxphys);
enum nested_sparse_fault nested_ept_sparse_hw_fault(struct nested_ept_sparse_hw *,uint64_t gpa,uint8_t access,struct nested_ept_result *);
bool nested_ept_sparse_hw_reset(struct nested_ept_sparse_hw *);
void nested_ept_sparse_hw_destroy(struct nested_ept_sparse_hw *);
#endif
