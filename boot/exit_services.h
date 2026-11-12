#ifndef SYLE_EXIT_SERVICES_H
#define SYLE_EXIT_SERVICES_H
#include <stddef.h>
#include <stdint.h>
/* Firmware-independent final-map transaction. Adapters normalize status only;
 * no output/allocations occur between a successful map read and EBS. */
enum syle_fw_status { SYLE_FW_OK, SYLE_FW_SMALL, SYLE_FW_STALE, SYLE_FW_ERROR };
struct syle_exit_ops {
    void *context;
    int (*map)(void *, void *, size_t *, size_t *, size_t *, uint32_t *);
    int (*allocate)(void *, size_t, void **);
    void (*release)(void *, void *);
    int (*exit)(void *, size_t);
};
struct syle_exit_result {
    void *map;
    size_t capacity, bytes, key, stride;
    uint32_t version;
    unsigned attempted, exited;
    int status;
};
#define SYLE_MAP_MAX (1024u * 1024u)
/* Three allocations and three EBS attempts. After the first EBS attempt,
 * only GetMemoryMap/ExitBootServices are permitted, even on failure. */
static inline int syle_exit_services(const struct syle_exit_ops *ops,
                                     struct syle_exit_result *r) {
    *r = (struct syle_exit_result){0};
    unsigned growth = 0;
    for (;;) {
        r->bytes = r->capacity;
        r->status = ops->map(ops->context, r->map, &r->bytes, &r->key,
                             &r->stride, &r->version);
        if (r->status == SYLE_FW_SMALL && !r->attempted) {
            if (growth++ == 3 || r->stride < 40 || r->stride > SYLE_MAP_MAX / 32 ||
                r->bytes <= r->capacity || r->bytes > SYLE_MAP_MAX - 32 * r->stride)
                break;
            size_t capacity = r->bytes + 32 * r->stride;
            void *next = NULL;
            if (ops->allocate(ops->context, capacity, &next) || !next) {
                r->status = SYLE_FW_ERROR;
                break;
            }
            if (r->map) ops->release(ops->context, r->map);
            r->map = next;
            r->capacity = capacity;
            continue;
        }
        if (r->status != SYLE_FW_OK) break;
        if (!r->map || !r->bytes || r->bytes > r->capacity || r->stride < 40 ||
            r->stride > r->bytes || r->stride % 8 || r->bytes % r->stride || r->version != 1) {
            r->status = SYLE_FW_ERROR;
            break;
        }
        ++r->attempted;
        r->status = ops->exit(ops->context, r->key);
        if (r->status == SYLE_FW_OK) { r->exited = 1; return 0; }
        if (r->status != SYLE_FW_STALE || r->attempted == 3) break;
    }
    /* Before EBS, normal cleanup is safe. After EBS, retain everything and
     * require the caller to halt using hardware-only diagnostics. */
    if (!r->attempted && r->map) {
        ops->release(ops->context, r->map);
        r->map = NULL;
        r->capacity = 0;
    }
    return -1;
}
#endif
