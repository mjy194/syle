#ifndef SYLE_NESTED_EPT_HW_H
#define SYLE_NESTED_EPT_HW_H
#include "nested_ept.h"
#include "ept.h"
struct nested_ept_hw {
    uint64_t tables[4],eptp;
    struct nested_ept_result plan[16];
    bool published;
};
bool nested_ept_hw_init(struct nested_ept_hw *c,struct ept_fixture *f);
bool nested_ept_hw_rebuild(struct nested_ept_hw *c,struct ept_fixture *f);
bool nested_ept_hw_flush(struct nested_ept_hw *c);
void nested_ept_hw_destroy(struct nested_ept_hw *c);
#endif
