#ifndef SYLE_NESTED_LOCAL_INVEPT_H
#define SYLE_NESTED_LOCAL_INVEPT_H
#include <stdint.h>
enum nested_local_invept_status {
    NESTED_LOCAL_INVEPT_OK,
    NESTED_LOCAL_INVEPT_INVALID28,
    NESTED_LOCAL_INVEPT_UNSUPPORTED
};
/* Pure classification for the advertised single-context (type 1) profile.
 * EPT capability contract: WB only, four-level only, AD disabled; UC/AD1/
 * five-level EPTP are INVALID28 here, not universally invalid on every CPU.
 * No memory reads, invalidation, ownership lookup or hardware side effects.
 * maxphys is the advertised guest physical width, restricted to 12..48;
 * out-of-range caller configuration returns UNSUPPORTED, not guest error28.
 * OK admits arbitrary aligned roots, including zero and roots without backing:
 * INVEPT invalidates translations; it does not walk the supplied EPT.
 * UNSUPPORTED must remain a monitor policy stop, never fabricated VMfail(28).
 * Caller separately enforces instruction privilege/operand fault ordering. */
enum nested_local_invept_status nested_local_invept_check(
    uint64_t type,uint64_t eptp,uint64_t descriptor_high,unsigned maxphys);
/* Explicit profile additionally advertising all-context (type 2). Type 2
 * ignores the entire low descriptor qword; high != 0 remains a policy stop.
 * Invalid monitor maxphys remains UNSUPPORTED for either advertised type.
 * The original single-context checker above retains its old contract. */
enum nested_local_invept_status nested_local_invept_check_allcontexts(
    uint64_t type,uint64_t eptp,uint64_t descriptor_high,unsigned maxphys);
#endif
