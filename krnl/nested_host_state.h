#ifndef SYLE_NESTED_HOST_STATE_H
#define SYLE_NESTED_HOST_STATE_H
#include <stdint.h>
#include <stdbool.h>
struct __attribute__((packed)) nested_host_table {
    uint16_t limit;
    uint64_t base;
};
struct nested_host_state {
    uint64_t cr0,cr3,cr4,fs_base,gs_base,pat,efer;
    uint64_t sysenter_cs,sysenter_esp,sysenter_eip,debugctl,dr7;
    struct nested_host_table gdtr,idtr;
    uint16_t cs,ss,ds,es,fs,gs,tr,ldtr;
};
/* Privileged current-CPU helpers. Caller gates CPU/MSR capabilities and owns
 * the CPU; IF must already be zero (these helpers never change it).
 * No CR2 here: entry-frame protection owns CR2 and diagnostic sentinels.
 * Capture failure leaves output untouched. Matches reads only; caller must
 * restore descriptor limits and other host state before invoking it. */
bool nested_host_state_capture(struct nested_host_state *out);
bool nested_host_state_matches(const struct nested_host_state *expected);
#endif
