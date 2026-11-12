#ifndef SYLE_USER_ELF_H
#define SYLE_USER_ELF_H
#include <stddef.h>
#include <stdint.h>
#define SYLE_USER_BASE UINT64_C(0x8000000000)
#define SYLE_USER_PAGE 4096u
/* Deliberately bounded ELF64 ABI: one RX page and optional one RW/NX page.
 * No interpreter, relocations, dynamic linking, overlapping segments or W+X.
 */
int user_elf_load(const void *image, size_t size, unsigned char *code,
                  unsigned char *data, uint64_t *entry);
#endif
