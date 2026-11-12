#ifndef SYLE_READ_BOUNDED_H
#define SYLE_READ_BOUNDED_H
#include <stddef.h>
/* Read an entire stream up to capacity. A short first read is accepted only
 * when the following read establishes EOF. Nonzero continuation is rejected,
 * never parsed as a silently truncated file. used is transactional. */
static inline int syle_read_bounded(void *context,
        int (*read)(void *, void *, size_t *), void *buffer,
        size_t capacity, size_t *used) {
    size_t bytes = capacity;
    if (read(context, buffer, &bytes) || bytes > capacity) return -1;
    unsigned char extra;
    size_t n = 1;
    if (read(context, &extra, &n) || n) return -1;
    *used = bytes;
    return 0;
}
#endif
