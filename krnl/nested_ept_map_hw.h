#ifndef SYLE_NESTED_EPT_MAP_HW_H
#define SYLE_NESTED_EPT_MAP_HW_H
#include "nested_ept_map.h"
#include "ept.h"
struct nested_ept_map_hw {
    uint64_t tables[4],eptp,generation;
    struct nested_ept_map committed;
    bool published,poisoned;
};
bool nested_ept_map_hw_init(struct nested_ept_map_hw *,struct ept_fixture *);
bool nested_ept_map_hw_rebuild(struct nested_ept_map_hw *,struct ept_fixture *);
bool nested_ept_map_hw_flush(struct nested_ept_map_hw *);
void nested_ept_map_hw_destroy(struct nested_ept_map_hw *);
#endif
