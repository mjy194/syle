#ifndef SYLE_BOOT_PLATFORM_H
#define SYLE_BOOT_PLATFORM_H
#include <stddef.h>

#define SYLE_BOOT_CONFIG_LIMIT 4096u
#define SYLE_BOOT_TITLE_LIMIT 80u

enum syle_boot_mode {
    SYLE_BOOT_NORMAL = 0,
    SYLE_BOOT_DIAGNOSTIC = 1
};
struct syle_boot_config {
    enum syle_boot_mode mode;
    unsigned timeout_seconds;
    char title[SYLE_BOOT_TITLE_LIMIT + 1];
};
/* Firmware adapter valid only during syle_boot_run, before kernel handoff.
 * No callback or context pointer belongs in the runtime boot_info protocol.
 * read_config: 0 found, 1 missing, negative I/O/oversize/error. Returns raw
 * bytes, not a C string. poll_key: ASCII, 0 pending, negative unavailable.
 * launch: synchronous, does not return on success; negative on pre-handoff
 * failure. Once boot services exit, it must never return to this function.
 */
struct syle_boot_platform {
    void *context;
    void (*log)(void *, const char *);
    int (*read_config)(void *, char *, size_t capacity, size_t *size);
    int (*poll_key)(void *);
    void (*stall_ms)(void *, unsigned milliseconds);
    int (*launch)(void *, const struct syle_boot_config *);
};
void syle_boot_defaults(struct syle_boot_config *);
/* Strict parser: all-or-nothing; output is unchanged on failure. */
int syle_boot_parse(const char *, size_t, struct syle_boot_config *);
int syle_boot_run(const struct syle_boot_platform *);
#endif
