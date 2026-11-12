#ifndef SYLE_NESTED_GUEST_H
#define SYLE_NESTED_GUEST_H
#include "ept.h"
bool nested_guest_run(uint64_t vmcs01,uint64_t basic,struct ept_fixture *fixture);
struct nested_vcpu;
bool nested_guest_run_context(struct nested_vcpu *,uint64_t,uint64_t,struct ept_fixture *);
#endif
