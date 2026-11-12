#ifndef SYLE_LINUX_PROFILE_PREPARE_H
#define SYLE_LINUX_PROFILE_PREPARE_H
#include "linux_profile_activation.h"
#include "linux_acpi_reduced_install.h"
struct lpp_context {
 struct guest_apic apic;struct ioapic io;struct gia irq;
 struct lams_state transactions;struct lio_state io_transactions;
 struct lad_config config;struct lgs_session *session;
 struct sg_memory_ops owner;uint64_t eptp;
 enum lai_state firmware;bool configured,busy,prepared,failed;
};
/* Fresh zero object, fresh globally unique nonzero domain per CREATE. Models
 * initialize here so lad_valid is available before manager registration.
 * Stable address required (internal self pointers), do not value-copy context.
 * Caller must destroy old entire resource domain before creating a new one. */
bool lpp_init(struct lpp_context*,uint64_t,void*,bool(*)(void*,uint64_t*));
/* Exact optional prepare_guest callback: after owned RAM creation, before VMX
 * enable. One attempt, sticky failure; never allocates/releases or enters VMX. */
bool lpp_prepare_guest(void*,struct lgs_session*);
#endif
