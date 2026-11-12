#ifndef STAGE0_CONSOLE_H
#define STAGE0_CONSOLE_H
#include <efi.h>
#include "../../include/syle/boot.h"
void console_snapshot(struct syle_framebuffer *out);

/* Call exactly once while Boot Services are alive. No allocations required. */
#ifndef SYLE_KERNEL
void console_init(EFI_BOOT_SERVICES *bs);
#endif
/* Bind a validated framebuffer snapshot without any firmware calls. */
void console_bind(const struct syle_framebuffer *framebuffer);
int console_available(void);
/* Screen only: caller can independently mirror characters to a UART. */
void console_putc(char c);
/* Clear screen and switch to a dark-red panic background. */
void console_panic_style(void);
#endif
