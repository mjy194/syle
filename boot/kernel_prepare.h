#ifndef SYLE_KERNEL_PREPARE_H
#define SYLE_KERNEL_PREPARE_H
#include "kernel_elf.h"
struct syle_kernel_allocator {
    void *context;
    int (*reserve)(void *, uint64_t *, size_t, void **);
    void (*release)(void *, uint64_t, size_t);
};
/* Transactional preparation; host destination may represent physical memory.
 * reserve must return both the physical address and writable mapping. */
static inline int syle_kernel_prepare(const void *file, size_t bytes,
        const struct syle_kernel_allocator *allocator, struct syle_kernel_plan *out) {
    struct syle_kernel_plan plan;
    if (syle_kernel_elf_inspect(file, bytes, &plan)) return -1;
    uint64_t address = plan.base;
    void *destination = NULL;
    if (allocator->reserve(allocator->context, &address, plan.size, &destination)) return -2;
    if (address != plan.base || !destination ||
        syle_kernel_elf_load(file, bytes, destination, plan.size, &plan)) {
        allocator->release(allocator->context, address, plan.size);
        return -3;
    }
    *out = plan;
    return 0;
}
#endif
