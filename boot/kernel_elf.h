#ifndef SYLE_KERNEL_ELF_H
#define SYLE_KERNEL_ELF_H
#include <stddef.h>
#include <stdint.h>
#define SYLE_KERNEL_BASE UINT64_C(0x04000000)
#define SYLE_KERNEL_MAX_SPAN (8u * 1024u * 1024u)
#define SYLE_KERNEL_MAX_FILE (5u * 1024u * 1024u)
struct syle_kernel_plan { uint64_t base, size, entry; };
/* Strict, bounded ELF64 ET_EXEC identity-addressed kernel image.
 * Return 0 on success, -1 on malformed/unsupported input. Plan is unchanged
 * on failure. base and size describe a page-aligned reserved physical span.
 */
int syle_kernel_elf_inspect(const void *image, size_t bytes,
                          struct syle_kernel_plan *out);
/* Destination represents plan.base (host memory in tests, exact physical
 * allocation in EFI). Validate completely before touching destination or out.
 * Source/destination cannot overlap. Caller supplies distinct valid output.
 */
int syle_kernel_elf_load(const void *image, size_t bytes, void *destination,
                       size_t capacity, struct syle_kernel_plan *out);
#endif
