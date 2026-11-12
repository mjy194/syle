#ifndef STAGE0_EXCEPTIONS_H
#define STAGE0_EXCEPTIONS_H
#include <efi.h>
void exceptions_init(void);
/* 0: mirrored output, 1: UART only, 2: framebuffer only. */
void log_output_mode(unsigned mode);
__attribute__((noreturn)) void panic(const char *message);
void log_text(const char *s);
void log_hex(UINT64 value);
#endif
