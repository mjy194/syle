#ifndef SYLE_FIRMWARE_CPU_PROFILE_H
#define SYLE_FIRMWARE_CPU_PROFILE_H
#include <stdbool.h>
#include <stdint.h>
struct fcp_host { uint32_t basic_edx,extended_edx,max_basic,max_extended;unsigned physical_bits,virtual_bits; };
struct fcp_result { uint32_t a,b,c,d; };
bool fcp_efer_allowed(uint64_t);
bool fcp_supported(const struct fcp_host*);
bool fcp_query(const struct fcp_host*,uint32_t leaf,uint32_t sub,struct fcp_result*);
#endif
