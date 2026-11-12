#include "../include/syle/boot_platform.h"

int syle_boot_run(const struct syle_boot_platform *p) {
    if (!p || !p->log || !p->read_config || !p->poll_key ||
        !p->stall_ms || !p->launch) return -1;
    struct syle_boot_config config;
    char buffer[SYLE_BOOT_CONFIG_LIMIT];
    size_t size = 0;
    syle_boot_defaults(&config);
    p->log(p->context, "SYLE.BOOT_ENTRY\n");
    int status = p->read_config(p->context, buffer, sizeof buffer, &size);
    if (status == 1) p->log(p->context, "BOOT.CONFIG_MISSING: using defaults\n");
    else if (status != 0 || syle_boot_parse(buffer, size, &config) != 0) {
        syle_boot_defaults(&config);
        config.mode = SYLE_BOOT_DIAGNOSTIC;
        config.timeout_seconds = 0;
        p->log(p->context, "BOOT.CONFIG_INVALID: diagnostic fallback\n");
    } else p->log(p->context, "BOOT.CONFIG_LOADED\n");
    p->log(p->context,
           "\n\n============================================================\n"
           "                         BOOT MENU\n"
           "============================================================\n\n  ");
    p->log(p->context, config.title);
    p->log(p->context,
           "\n\n  [1]  Start Syle management\n"
           "       Load kernel and enter the user-mode program.\n\n"
           "  [2]  Diagnostic halt\n"
           "       Initialize kernel diagnostics, then stop.\n\n"
           "------------------------------------------------------------\n"
           "  Press 1 or 2 to start your selection.\n"
           "  Enter: configured default; timeout: automatic default\n  ");
    p->log(p->context, config.mode == SYLE_BOOT_NORMAL ?
           "Default: normal; timeout seconds: " : "Default: diagnostic; timeout seconds: ");
    char seconds[3];
    if (config.timeout_seconds >= 10) {
        seconds[0] = (char)('0' + config.timeout_seconds / 10);
        seconds[1] = (char)('0' + config.timeout_seconds % 10);
        seconds[2] = 0;
    } else {
        seconds[0] = (char)('0' + config.timeout_seconds);
        seconds[1] = 0;
    }
    p->log(p->context, seconds);
    p->log(p->context, "\n\n============================================================\n\n");
    /* No timer/interrupt dependency in boot policy. Firmware adapter provides
     * bounded 100 ms stalls. No input protocol means immediate default. */
    unsigned ticks = config.timeout_seconds * 10;
    while (ticks) {
        int key = p->poll_key(p->context);
        if (key < 0) {
            p->log(p->context, "BOOT.INPUT_UNAVAILABLE: automatic default\n");
            break;
        }
        if (key == '1' || key == '2') {
            config.mode = key == '1' ? SYLE_BOOT_NORMAL : SYLE_BOOT_DIAGNOSTIC;
            break;
        }
        if (key == '\r' || key == '\n') break;
        p->stall_ms(p->context, 100);
        --ticks;
    }
    p->log(p->context, config.mode == SYLE_BOOT_NORMAL ?
           "BOOT.SELECTED=normal\n" : "BOOT.SELECTED=diagnostic\n");
    status = p->launch(p->context, &config);
    p->log(p->context, "BOOT.LAUNCH_FAILED\n");
    return status < 0 ? status : -1;
}
